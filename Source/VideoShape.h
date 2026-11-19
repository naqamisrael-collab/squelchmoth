#pragma once
// Squelchmoth 1.34 - the picture's shape: as the video is, or vertical / square / tall / wide, filled (cropped around a
// focus point) or fitted (bars). One drawing function serves the Video window, the poster picture and the checks.
#include <JuceHeader.h>
#include "SqShape.h"
#include "SqText.h"
#include "VideoText.h"

namespace VideoShape
{
    namespace te = tracktion;

    struct Setting { SqShape::Shape shape; bool fill = true; double focusX = 0.5, focusY = 0.5; bool active() const { return shape.given(); } };

    inline Setting read (te::Edit& e)
    {
        Setting s; s.shape = SqShape::parse (e.state["frameShape"].toString().toStdString());
        s.fill = e.state.getProperty ("frameFit", "fill").toString() != "fit";
        s.focusX = juce::jlimit (0.0, 1.0, (double) e.state.getProperty ("frameFocusX", 0.5)); s.focusY = juce::jlimit (0.0, 1.0, (double) e.state.getProperty ("frameFocusY", 0.5));
        return s;
    }

    inline void write (te::Edit& e, const Setting& s)
    {
        auto& um = e.getUndoManager(); um.beginNewTransaction ("Picture shape");
        if (s.shape.given()) e.state.setProperty ("frameShape", juce::String (s.shape.w) + ":" + juce::String (s.shape.h), &um); else e.state.removeProperty ("frameShape", &um);
        e.state.setProperty ("frameFit", s.fill ? "fill" : "fit", &um);
        e.state.setProperty ("frameFocusX", juce::jlimit (0.0, 1.0, s.focusX), &um); e.state.setProperty ("frameFocusY", juce::jlimit (0.0, 1.0, s.focusY), &um);
    }

    /** Draws a video frame into the shape's rectangle: cropped around the focus (fill), or whole with bars (fit). */
    inline void drawPicture (juce::Graphics& g, juce::Rectangle<float> frame, const juce::Image& src, const Setting& s)
    {
        if (! src.isValid()) return;
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        if (! s.active()) { g.drawImage (src, frame, juce::RectanglePlacement::centred); return; }
        if (s.fill)
        {
            const auto c = SqShape::crop (src.getWidth(), src.getHeight(), s.shape, s.focusX, s.focusY);
            if (c.w > 0) g.drawImage (src, juce::roundToInt (frame.getX()), juce::roundToInt (frame.getY()), juce::roundToInt (frame.getWidth()), juce::roundToInt (frame.getHeight()), c.x, c.y, c.w, c.h);
        }
        else
        {
            g.setColour (juce::Colours::black); g.fillRect (frame);
            g.drawImage (src, frame, juce::RectanglePlacement::centred);
        }
    }

    /** One frame exactly as it is exported: the picture in its shape with the words that show at that time. */
    inline juce::Image compose (const juce::Image& src, const Setting& s, int width, int height, const std::vector<SqText::Item>& texts, double sessionTime)
    {
        juce::Image out (juce::Image::RGB, juce::jmax (2, width), juce::jmax (2, height), true, juce::SoftwareImageType());
        juce::Graphics g (out); g.fillAll (juce::Colours::black);
        const auto frame = out.getBounds().toFloat();
        if (s.active()) drawPicture (g, frame, src, s);
        else if (src.isValid()) { const auto r = SqShape::fitInside (src.getWidth(), src.getHeight(), out.getWidth(), out.getHeight()); g.drawImage (src, juce::Rectangle<float> ((float) r.x, (float) r.y, (float) r.w, (float) r.h), juce::RectanglePlacement::stretchToFit); }
        VideoText::drawShowing (g, frame, texts, sessionTime);
        return out;
    }

    inline juce::String describe (const Setting& s)
    {
        if (! s.active()) return "The picture keeps the video's own shape.";
        int w = 0, h = 0; SqShape::size (s.shape, w, h);
        return "The picture is " + juce::String (SqShape::name (s.shape)) + " (" + juce::String (w) + " x " + juce::String (h) + " unless a size is given), "
               + (s.fill ? "filled: cropped around a focus " + juce::String (juce::roundToInt (s.focusX * 100)) + " % across and " + juce::String (juce::roundToInt (s.focusY * 100)) + " % down." : juce::String ("fitted: the whole picture with black bars."));
    }
}
