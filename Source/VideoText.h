#pragma once
// Squelchmoth 1.33 - words on the picture: kept in the session, drawn the same way in the Video window and in the export.
#include <JuceHeader.h>
#include "SqText.h"

namespace VideoText
{
    namespace te = tracktion;
    using Item = SqText::Item;

    inline const juce::Identifier& listId() { static const juce::Identifier i ("FRAMETEXT"); return i; }
    inline const juce::Identifier& itemId() { static const juce::Identifier i ("T"); return i; }

    /** The session's texts, in the order they were made. */
    inline std::vector<Item> read (te::Edit& e)
    {
        std::vector<Item> out; const auto list = e.state.getChildWithName (listId());
        for (const auto& c : list)
        {
            if (! c.hasType (itemId())) continue;
            Item it; it.start = (double) c.getProperty ("s", 0.0); it.end = (double) c.getProperty ("e", 0.0); it.text = c["text"].toString().toStdString();
            it.style = (int) c.getProperty ("style", 0); it.place = (int) c.getProperty ("place", 0); it.size = (double) c.getProperty ("size", 1.0);
            it.colour = (unsigned int) (juce::int64) c.getProperty ("colour", (juce::int64) 0xffffffff);
            out.push_back (it);
        }
        return out;
    }

    /** Replaces the session's texts. One undo step. */
    inline void write (te::Edit& e, std::vector<Item> items, const juce::String& what)
    {
        auto& um = e.getUndoManager(); um.beginNewTransaction (what);
        auto list = e.state.getOrCreateChildWithName (listId(), &um);
        list.removeAllChildren (&um);
        for (auto& it : items)
        {
            SqText::tidy (it);
            juce::ValueTree c (itemId());
            c.setProperty ("s", it.start, nullptr); c.setProperty ("e", it.end, nullptr); c.setProperty ("text", juce::String::fromUTF8 (it.text.c_str()), nullptr);
            c.setProperty ("style", it.style, nullptr); c.setProperty ("place", it.place, nullptr); c.setProperty ("size", it.size, nullptr); c.setProperty ("colour", (juce::int64) it.colour, nullptr);
            list.appendChild (c, &um);
        }
    }

    /** Draws one text into a frame (the picture's rectangle). The Video window and the export both use this. */
    inline void draw (juce::Graphics& g, juce::Rectangle<float> frame, const Item& it)
    {
        if (it.text.empty() || frame.getHeight() < 8.0f) return;
        const auto words = juce::String::fromUTF8 (it.text.c_str());
        const bool isCaption = it.style == SqText::caption, isLower = it.style == SqText::lowerThird;
        const float maxW = frame.getWidth() * (isLower ? 0.7f : 0.9f);
        float px = juce::jmax (4.0f, frame.getHeight() * (float) SqText::heightFraction (it));
        juce::TextLayout layout;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            juce::AttributedString as; as.setJustification (isLower ? juce::Justification::centredLeft : juce::Justification::centred); as.setWordWrap (juce::AttributedString::byWord);
            as.append (words, juce::Font (juce::FontOptions (px, isCaption ? juce::Font::plain : juce::Font::bold)), juce::Colour ((juce::uint32) it.colour));
            layout = juce::TextLayout(); layout.createLayout (as, maxW);
            // captions keep to three lines, titles to the frame: shrink a little when the words do not fit
            if ((isCaption ? layout.getNumLines() <= 3 : layout.getHeight() <= frame.getHeight() * 0.8f) || px <= 5.0f) break;
            px *= 0.85f;
        }
        const float w = juce::jmin (maxW, layout.getWidth()), h = layout.getHeight(), margin = frame.getHeight() * 0.07f, pad = px * 0.35f;
        const int place = SqText::placeOf (it);
        const float y = place == SqText::top ? frame.getY() + margin : place == SqText::middle ? frame.getCentreY() - h * 0.5f : frame.getBottom() - margin - h;
        const float x = isLower ? frame.getX() + frame.getWidth() * 0.06f : frame.getCentreX() - maxW * 0.5f;
        const juce::Rectangle<float> area (x, y, maxW, h);
        const juce::Rectangle<float> inked (isLower ? x : frame.getCentreX() - w * 0.5f, y, w, h);
        if (isCaption || isLower)
        {
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (inked.expanded (pad * 1.6f, pad), px * 0.18f);
            if (isLower) { g.setColour (juce::Colour (0xffe8c46a)); g.fillRect (inked.getX() - pad * 1.6f, inked.getY() - pad, juce::jmax (2.0f, px * 0.14f), inked.getHeight() + pad * 2.0f); }
        }
        else
        {
            // a title has no band: a soft dark edge keeps it readable on any picture
            juce::AttributedString sh; sh.setJustification (juce::Justification::centred); sh.setWordWrap (juce::AttributedString::byWord);
            sh.append (words, juce::Font (juce::FontOptions (px, juce::Font::bold)), juce::Colours::black.withAlpha (0.75f));
            juce::TextLayout shadow; shadow.createLayout (sh, maxW);
            const float d = juce::jmax (1.0f, px * 0.05f);
            for (auto off : { juce::Point<float> (d, d), juce::Point<float> (-d, d), juce::Point<float> (d, -d), juce::Point<float> (-d, -d) }) shadow.draw (g, area.translated (off.x, off.y));
        }
        layout.draw (g, area);
    }

    /** Draws every text that shows at a session time. */
    inline void drawShowing (juce::Graphics& g, juce::Rectangle<float> frame, const std::vector<Item>& items, double t)
    {
        for (int i : SqText::showing (items, t)) draw (g, frame, items[(size_t) i]);
    }

    /** One text as a see-through picture the size of the frame, for laying over the video in an export. */
    inline juce::Image layer (const Item& it, int width, int height)
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (2, width), juce::jmax (2, height), true, juce::SoftwareImageType());
        { juce::Graphics g (img); draw (g, img.getBounds().toFloat(), it); }
        return img;
    }

    /** A line about a text, for lists. */
    inline juce::String line (int number, const Item& it)
    {
        return juce::String (number) + ". " + juce::String (it.start, 2) + " to " + juce::String (it.end, 2) + " s  " + SqText::styleName (it.style)
               + (it.place != SqText::automatic ? juce::String (" (") + SqText::placeName (it.place) + ")" : juce::String()) + ": " + juce::String::fromUTF8 (it.text.c_str()).replace ("\n", " / ");
    }
}
