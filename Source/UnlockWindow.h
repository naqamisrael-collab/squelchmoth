#pragma once
// Help > Licences and Upgrades (1.9 "Store"): what this copy owns, every product with its price and a Buy button,
// and the key field. Opened by every lock in the app (a locked plug-in panel, a locked menu item, a locked window),
// with the product that unlocks it highlighted.
#include "Edition.h"
#include "Theme.h"

class UnlockContent : public juce::Component
{
public:
    std::function<juce::String (const juce::String&)> onActivate;   // returns "" or an error
    std::function<juce::StringArray()> ownedKeys;                   // "CORE  serial 1A2B3C4D" lines
    std::function<void()> onReopen;                                 // reopen the session so placeholders become the real plug-ins

    UnlockContent()
    {
        if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::logo_svg, BinaryData::logo_svgSize))) logo = juce::Drawable::createFromSVG (*svg);
        for (auto& p : Edition::products())
        {
            auto* c = cards.add (new Card (p));
            addAndMakeVisible (c);
        }
        keyField.setTextToShowWhenEmpty ("Paste your key here (it starts with SQM2-)", juce::Colour (0xff8f84aa));
        keyField.setFont (Theme::font (14.0f));
        keyField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff17121f));
        keyField.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3150));
        keyField.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        keyField.onReturnKey = [this] { activate(); };
        addAndMakeVisible (keyField);
        activateBtn.setButtonText ("ACTIVATE");
        activateBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd4ff3a)); activateBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        activateBtn.onClick = [this] { activate(); };
        addAndMakeVisible (activateBtn);
        reopenBtn.setButtonText ("REOPEN SESSION TO UNLOCK");
        reopenBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x33ffffff));
        reopenBtn.onClick = [this] { if (onReopen) onReopen(); };
        addChildComponent (reopenBtn);
        setSize (980, 640);
        refresh();
    }

    /** Highlight the product a lock pointed at (its card glows, the page scrolls to nothing - everything fits). */
    void focusProduct (const juce::String& code)   { focus = code; for (auto* c : cards) c->highlight = code == c->product.code; refresh(); }

    void refresh()
    {
        for (auto* c : cards) { c->owned = owns (c->product.code); c->buy.setVisible (! c->owned); c->repaint(); }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0d0a16));
        auto r = getLocalBounds().toFloat();
        auto band = r.removeFromTop (86);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1450), band.getX(), band.getY(), juce::Colour (0xff0d0a16), band.getRight(), band.getBottom(), false));
        g.fillRect (band);
        g.setColour (juce::Colour (0xffd4ff3a)); g.fillRect (band.removeFromBottom (2));
        if (logo) logo->drawWithin (g, juce::Rectangle<float> (22, 14, 56, 56), juce::RectanglePlacement::centred, 1.0f);
        g.setColour (juce::Colours::white); g.setFont (Theme::font (24.0f, true));
        g.drawText ("LICENCES AND UPGRADES", 92, 16, 600, 30, juce::Justification::left);
        g.setColour (juce::Colour (0xffcfc6e8)); g.setFont (Theme::font (13.5f));
        g.drawText ("This Mac runs: " + Edition::editionName() + (Edition::isDemo() ? "   (free forever - the basics)" : ""), 92, 46, 700, 20, juce::Justification::left);
        g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (12.0f));
        g.drawText ("Buy in the store - your key arrives by email straight away and works offline.", 92, 64, 760, 16, juce::Justification::left);
        // key area
        g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (11.5f, true));
        g.drawText ("ENTER A KEY", keyLabel, juce::Justification::left);
        g.setFont (Theme::font (12.0f));
        g.setColour (status.startsWith ("!") ? juce::Colour (0xffff6a7a) : juce::Colour (0xffd4ff3a));
        g.drawFittedText (status.trimCharactersAtStart ("!"), statusArea, juce::Justification::left, 2);
        if (ownedKeys)
        {
            auto ks = ownedKeys();
            g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (11.0f));
            g.drawFittedText (ks.isEmpty() ? juce::String ("No keys on this Mac yet.") : "Keys on this Mac: " + ks.joinIntoString ("  |  "), keysArea, juce::Justification::left, 2);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20); r.removeFromTop (86);
        auto bottom = r.removeFromBottom (118);
        r.removeFromBottom (10);
        // cards: Squelchmoth and Complete on the first row (wide), the five packs on the second
        auto row1 = r.removeFromTop ((r.getHeight() - 12) / 2); r.removeFromTop (12);
        cards[0]->setBounds (row1.removeFromLeft ((row1.getWidth() - 12) / 2)); row1.removeFromLeft (12); cards[1]->setBounds (row1);
        const int w = (r.getWidth() - 4 * 10) / 5;
        for (int i = 2; i < cards.size(); ++i) { cards[i]->setBounds (r.removeFromLeft (w)); r.removeFromLeft (10); }
        keyLabel = bottom.removeFromTop (18);
        auto line = bottom.removeFromTop (38);
        activateBtn.setBounds (line.removeFromRight (140)); line.removeFromRight (10);
        keyField.setBounds (line);
        bottom.removeFromTop (6);
        auto st = bottom.removeFromTop (30);
        reopenBtn.setBounds (st.removeFromRight (240).withHeight (28));
        statusArea = st;
        keysArea = bottom;
    }

private:
    struct Card : public juce::Component
    {
        explicit Card (const Edition::Product& p) : product (p)
        {
            buy.setButtonText ("BUY  " + juce::String (p.price));
            buy.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd4ff3a)); buy.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
            buy.onClick = [this] { juce::URL (Edition::storeUrl (product.code)).launchInDefaultBrowser(); };
            addAndMakeVisible (buy);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1);
            const bool big = juce::String (product.code) == "CORE" || juce::String (product.code) == "COMPLETE";
            g.setColour (owned ? juce::Colour (0xff142218) : juce::Colour (0xff17121f)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (highlight ? juce::Colour (0xffd4ff3a) : (juce::String (product.code) == "COMPLETE" ? juce::Colour (0xffb06bff) : juce::Colour (0xff3a3150)));
            g.drawRoundedRectangle (r, 10.0f, highlight ? 2.5f : 1.2f);
            auto c = r.reduced (14, 12);
            auto pill = c.removeFromTop (18);
            g.setFont (Theme::font (10.5f, true));
            const juce::String tag = owned ? "OWNED" : (juce::String (product.code) == "COMPLETE" ? "BEST VALUE" : "LOCKED");
            const auto pw = (float) juce::GlyphArrangement::getStringWidth (Theme::font (10.5f, true), tag) + 16.0f;
            g.setColour (owned ? juce::Colour (0xff3ad17a) : (tag == "BEST VALUE" ? juce::Colour (0xffb06bff) : juce::Colour (0xff4a4060)));
            g.fillRoundedRectangle (pill.removeFromLeft (pw), 9.0f);
            g.setColour (juce::Colours::white); g.drawText (tag, juce::Rectangle<float> (c.getX(), c.getY() - 18, pw, 18), juce::Justification::centred);
            c.removeFromTop (6);
            g.setFont (Theme::font (big ? 20.0f : 15.0f, true));
            g.drawFittedText (product.name, c.removeFromTop (big ? 26 : 38).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
            g.setColour (juce::Colour (0xffcfc6e8)); g.setFont (Theme::font (big ? 12.5f : 11.0f));
            g.drawFittedText (product.blurb, c.removeFromTop (big ? 36 : 84).toNearestInt(), juce::Justification::topLeft, big ? 2 : 6, 1.0f);
            auto priceRow = c.removeFromTop (24);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (big ? 22.0f : 17.0f, true));
            g.drawText (product.price, priceRow, juce::Justification::left);
            const float px = (float) juce::GlyphArrangement::getStringWidth (Theme::font (big ? 22.0f : 17.0f, true), product.price) + 8.0f;
            g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (12.0f));
            auto lr = priceRow.withTrimmedLeft (px); g.drawText (product.list, lr, juce::Justification::left);
            const float lw = (float) juce::GlyphArrangement::getStringWidth (Theme::font (12.0f), product.list);
            g.drawLine (lr.getX(), lr.getCentreY(), lr.getX() + lw, lr.getCentreY(), 1.2f);
        }
        void resized() override   { auto r = getLocalBounds().reduced (14, 12); buy.setBounds (r.removeFromBottom (30)); buy.setVisible (! owned); }
        const Edition::Product& product; juce::TextButton buy; bool owned = false, highlight = false;
    };
    static bool owns (const juce::String& code)
    {
        auto& o = Edition::owned();
        if (code == "CORE") return o.core;
        if (code == "COMPLETE") return o.core && o.vocal && o.mastering && o.instruments && o.beat && o.creative;
        if (code == "VOCAL") return o.vocal; if (code == "MASTER") return o.mastering; if (code == "INSTR") return o.instruments;
        if (code == "BEAT") return o.beat; if (code == "CREATIVE") return o.creative;
        return false;
    }
    void activate()
    {
        if (! onActivate) return;
        auto err = onActivate (keyField.getText());
        if (err.isEmpty())
        {
            status = "Unlocked: " + Edition::editionName() + ". Plug-ins already in this session unlock when it is reopened.";
            keyField.clear(); reopenBtn.setVisible (true);
            for (auto* c : cards) c->resized();
        }
        else status = "!" + err;
        refresh();
    }
    juce::OwnedArray<Card> cards; juce::TextEditor keyField; juce::TextButton activateBtn, reopenBtn;
    std::unique_ptr<juce::Drawable> logo; juce::String status, focus; juce::Rectangle<int> keyLabel, statusArea, keysArea;
};
