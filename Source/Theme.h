#pragma once
// Squelchmoth visual theme. Every colour is a live setting (Preferences > Display) saved in the
// app's settings file, so the Edit and Mix window backgrounds, panels, accents and the default
// track-colour palette can all be changed by the user. Built-in presets: Moth (default), Carbon,
// Daylight, Midnight Blue, Studio Grey.

#include "Session.h"

namespace Theme
{
    struct Colours
    {
        juce::Colour bg        { 0xff0d0714 };   // Edit window background
        juce::Colour mixBg     { 0xff0d0714 };   // Mix window background
        juce::Colour panel     { 0xff160d22 };
        juce::Colour panel2    { 0xff1e1330 };
        juce::Colour line      { 0xff2e2147 };
        juce::Colour text      { 0xffece6ff };
        juce::Colour dim       { 0xff8f84aa };
        juce::Colour acid      { 0xffd4ff3a };   // primary accent (playhead, selection, meters)
        juce::Colour mint      { 0xff3affb0 };
        juce::Colour magenta   { 0xffff3ad1 };
        juce::Colour violet    { 0xff7a3aff };   // buttons / selected
        juce::Colour red       { 0xffff4a5a };
        juce::Colour amber     { 0xffffb23a };
        juce::Colour waveform  { 0x00000000 };   // transparent = use the track colour
        juce::Colour grid      { 0x00000000 };   // transparent = derive from line
        juce::Array<juce::Colour> trackPalette {
            juce::Colour (0xffd4ff3a), juce::Colour (0xff3affb0), juce::Colour (0xffff3ad1), juce::Colour (0xff7a3aff),
            juce::Colour (0xffffb23a), juce::Colour (0xffff4a5a), juce::Colour (0xff3ad1ff), juce::Colour (0xffb0b0c0),
            juce::Colour (0xff8aff6a), juce::Colour (0xffff8a3a), juce::Colour (0xffe03aff), juce::Colour (0xff3a7aff),
            juce::Colour (0xffffe03a), juce::Colour (0xff3affe0), juce::Colour (0xffff6aa0), juce::Colour (0xffa0ff3a) };
        juce::String name { "Moth" };
        float uiScale = 1.0f;
    };

    inline Colours& current()  { static Colours c; return c; }

    inline juce::Colour bg()        { return current().bg; }
    inline juce::Colour mixBg()     { return current().mixBg; }
    inline juce::Colour panel()     { return current().panel; }
    inline juce::Colour panel2()    { return current().panel2; }
    inline juce::Colour line()      { return current().line; }
    inline juce::Colour text()      { return current().text; }
    inline juce::Colour dim()       { return current().dim; }
    inline juce::Colour acid()      { return current().acid; }
    inline juce::Colour mint()      { return current().mint; }
    inline juce::Colour magenta()   { return current().magenta; }
    inline juce::Colour violet()    { return current().violet; }
    inline juce::Colour red()       { return current().red; }
    inline juce::Colour amber()     { return current().amber; }
    inline juce::Colour gridColour(){ auto g = current().grid; return g.isTransparent() ? line() : g; }
    inline juce::Colour waveformFor (juce::Colour trackColour) { auto w = current().waveform; return w.isTransparent() ? trackColour : w; }
    inline const juce::Array<juce::Colour>& trackPalette() { return current().trackPalette; }
    inline juce::Colour nextTrackColour (int index) { auto& p = trackPalette(); return p[juce::jmax (0, index) % juce::jmax (1, p.size())]; }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h * current().uiScale, bold ? juce::Font::bold : juce::Font::plain));
    }

    //==========================================================================
    struct Preset { const char* name; juce::uint32 bg, mixBg, panel, panel2, line, text, dim, acid, mint, magenta, violet; };

    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Moth",          0xff0d0714, 0xff0d0714, 0xff160d22, 0xff1e1330, 0xff2e2147, 0xffece6ff, 0xff8f84aa, 0xffd4ff3a, 0xff3affb0, 0xffff3ad1, 0xff7a3aff },
            { "Carbon",        0xff17181b, 0xff141518, 0xff202227, 0xff2a2d33, 0xff383c44, 0xffe8e9ec, 0xff9aa0ab, 0xff5ec8ff, 0xff6fe3a1, 0xffff7a59, 0xff3d7bff },
            { "Daylight",      0xfff2f1f5, 0xffebeaf0, 0xffffffff, 0xffe6e4ec, 0xffcfcbd9, 0xff1d1a2b, 0xff6b6680, 0xff2a7dff, 0xff0f9d6a, 0xffc7129c, 0xff5b2bd6 },
            { "Midnight Blue", 0xff0a1020, 0xff080d1a, 0xff111a30, 0xff172240, 0xff243356, 0xffe6ecff, 0xff8a97bf, 0xff4fd1ff, 0xff5dffc3, 0xffff5db1, 0xff5a6dff },
            { "Studio Grey",   0xff2b2b2b, 0xff262626, 0xff333333, 0xff3d3d3d, 0xff4a4a4a, 0xfff0f0f0, 0xffa8a8a8, 0xfff2c14e, 0xff8fd18f, 0xffe77c7c, 0xff6b8fd6 } };
        return p;
    }

    inline void applyPreset (const Preset& p)
    {
        auto& c = current();
        c.name = p.name; c.bg = juce::Colour (p.bg); c.mixBg = juce::Colour (p.mixBg); c.panel = juce::Colour (p.panel);
        c.panel2 = juce::Colour (p.panel2); c.line = juce::Colour (p.line); c.text = juce::Colour (p.text); c.dim = juce::Colour (p.dim);
        c.acid = juce::Colour (p.acid); c.mint = juce::Colour (p.mint); c.magenta = juce::Colour (p.magenta); c.violet = juce::Colour (p.violet);
    }

    inline void save (juce::PropertiesFile& props)
    {
        auto& c = current();
        auto put = [&] (const char* k, juce::Colour col) { props.setValue (juce::String ("theme_") + k, col.toString()); };
        put ("bg", c.bg); put ("mixBg", c.mixBg); put ("panel", c.panel); put ("panel2", c.panel2); put ("line", c.line);
        put ("text", c.text); put ("dim", c.dim); put ("acid", c.acid); put ("mint", c.mint); put ("magenta", c.magenta);
        put ("violet", c.violet); put ("red", c.red); put ("amber", c.amber); put ("waveform", c.waveform); put ("grid", c.grid);
        juce::StringArray pal;
        for (auto col : c.trackPalette) pal.add (col.toString());
        props.setValue ("theme_palette", pal.joinIntoString (","));
        props.setValue ("theme_name", c.name);
        props.setValue ("theme_uiScale", c.uiScale);
        props.saveIfNeeded();
    }

    inline void load (juce::PropertiesFile& props)
    {
        auto& c = current();
        auto get = [&] (const char* k, juce::Colour& col) { auto s = props.getValue (juce::String ("theme_") + k); if (s.isNotEmpty()) col = juce::Colour::fromString (s); };
        get ("bg", c.bg); get ("mixBg", c.mixBg); get ("panel", c.panel); get ("panel2", c.panel2); get ("line", c.line);
        get ("text", c.text); get ("dim", c.dim); get ("acid", c.acid); get ("mint", c.mint); get ("magenta", c.magenta);
        get ("violet", c.violet); get ("red", c.red); get ("amber", c.amber); get ("waveform", c.waveform); get ("grid", c.grid);
        auto pal = juce::StringArray::fromTokens (props.getValue ("theme_palette"), ",", "");
        if (pal.size() >= 4) { c.trackPalette.clear(); for (auto& s : pal) c.trackPalette.add (juce::Colour::fromString (s)); }
        c.name = props.getValue ("theme_name", c.name);
        c.uiScale = (float) props.getDoubleValue ("theme_uiScale", 1.0);
    }

    /** Listeners (the main window) repaint everything when the theme changes. */
    inline juce::ChangeBroadcaster& broadcaster()   { static juce::ChangeBroadcaster b; return b; }
    inline void changed()                           { broadcaster().sendChangeMessage(); }
}

namespace Ui
{
    /** A non-blocking message box. (juce::AlertWindow::showMessageBoxAsync without a callback runs a nested
        modal loop when JUCE_MODAL_LOOPS_PERMITTED is on, which freezes whatever called it - a menu command or a
        Claude tool call - until OK is clicked.) */
    inline void message (juce::MessageBoxIconType icon, const juce::String& title, const juce::String& text)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (icon).withTitle (title).withMessage (text).withButton ("OK"),
                                      [] (int) {});
    }
}

class SquelchmothLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SquelchmothLookAndFeel()    { refresh(); }

    void refresh()
    {
        auto scheme = juce::LookAndFeel_V4::ColourScheme (
            Theme::bg(), Theme::panel(), Theme::panel2(), Theme::line(), Theme::text(),
            Theme::violet(), Theme::text(), Theme::magenta(), Theme::text());
        setColourScheme (scheme);

        setColour (juce::TextButton::buttonColourId, Theme::panel2());
        setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        setColour (juce::TextButton::textColourOffId, Theme::text());
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, Theme::text());
        setColour (juce::ToggleButton::tickColourId, Theme::acid());
        setColour (juce::Slider::thumbColourId, Theme::acid());
        setColour (juce::Slider::trackColourId, Theme::violet());
        setColour (juce::Slider::rotarySliderFillColourId, Theme::acid());
        setColour (juce::Slider::rotarySliderOutlineColourId, Theme::line());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxTextColourId, Theme::text());
        setColour (juce::Label::textColourId, Theme::text());
        setColour (juce::TextEditor::backgroundColourId, Theme::panel2());
        setColour (juce::TextEditor::textColourId, Theme::text());
        setColour (juce::TextEditor::outlineColourId, Theme::line());
        setColour (juce::TextEditor::highlightColourId, Theme::violet().withAlpha (0.6f));
        setColour (juce::PopupMenu::backgroundColourId, Theme::panel());
        setColour (juce::PopupMenu::textColourId, Theme::text());
        setColour (juce::PopupMenu::headerTextColourId, Theme::dim());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::violet());
        setColour (juce::ResizableWindow::backgroundColourId, Theme::bg());
        setColour (juce::ScrollBar::thumbColourId, Theme::line().brighter (0.3f));
        setColour (juce::ListBox::backgroundColourId, Theme::panel());
        setColour (juce::ListBox::textColourId, Theme::text());
        setColour (juce::ComboBox::backgroundColourId, Theme::panel2());
        setColour (juce::ComboBox::textColourId, Theme::text());
        setColour (juce::ComboBox::outlineColourId, Theme::line());
        setColour (juce::ComboBox::arrowColourId, Theme::dim());
        setColour (juce::TabbedButtonBar::tabTextColourId, Theme::dim());
        setColour (juce::TabbedButtonBar::frontTextColourId, Theme::text());
        setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
        setColour (juce::TabbedComponent::outlineColourId, Theme::line());
        setColour (juce::AlertWindow::backgroundColourId, Theme::panel());
        setColour (juce::AlertWindow::textColourId, Theme::text());
        setColour (juce::TableHeaderComponent::backgroundColourId, Theme::panel2());
        setColour (juce::TableHeaderComponent::textColourId, Theme::text());
        setColour (juce::TableHeaderComponent::outlineColourId, Theme::line());
        setColour (juce::TooltipWindow::backgroundColourId, Theme::panel2());
        setColour (juce::TooltipWindow::textColourId, Theme::text());
        setColour (juce::TooltipWindow::outlineColourId, Theme::line());
        setColour (juce::DocumentWindow::textColourId, Theme::text());
        setColour (juce::PropertyComponent::backgroundColourId, Theme::panel());
        setColour (juce::PropertyComponent::labelTextColourId, Theme::text());
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgCol,
                               bool over, bool down) override
    {
        // 1.10 console finish: a lit top edge, a soft vertical gradient and a shadowed base, so every button reads as a
        // physical key. "On" buttons get a glow ring in their own colour. Colours still come from the theme.
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float radius = juce::jmin (5.0f, r.getHeight() * 0.3f);
        const bool on = b.getToggleState();
        auto c = bgCol;
        if (down) c = c.darker (0.12f); else if (over) c = c.brighter (0.10f);
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.55f);
        g.setGradientFill (juce::ColourGradient (c.brighter (down ? 0.0f : 0.16f), r.getX(), r.getY(), c.darker (0.22f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        if (! down)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.22f : 0.10f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + radius, r.getRight() - radius);
        if (on)
        {
            g.setColour (bgCol.brighter (0.5f).withAlpha (0.9f));
            g.drawRoundedRectangle (r, radius, 1.2f);
            g.setColour (bgCol.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 1.5f);
        }
        else
        {
            g.setColour (over ? Theme::line().brighter (0.45f) : Theme::line().brighter (0.15f));
            g.drawRoundedRectangle (r, radius, 1.0f);
        }

    }

    // Dialogs: a clean console-style panel with a compact status badge (the stock V4 badge is a huge,
    // clipped circle).
    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout) override
    {
        auto b = alert.getLocalBounds().toFloat();
        g.fillAll (Theme::panel());
        juce::Colour accent = Theme::violet();
        juce::String glyph;
        switch (alert.getAlertType())
        {
            case juce::MessageBoxIconType::WarningIcon:  accent = Theme::amber(); glyph = "!"; break;
            case juce::MessageBoxIconType::InfoIcon:     accent = Theme::mint();  glyph = "i"; break;
            case juce::MessageBoxIconType::QuestionIcon: accent = Theme::violet(); glyph = "?"; break;
            case juce::MessageBoxIconType::NoIcon:       default: break;
        }
        g.setColour (accent);
        g.fillRect (b.removeFromTop (3.0f));
        if (glyph.isNotEmpty())
        {
            juce::Rectangle<float> badge (18.0f, 20.0f, 38.0f, 38.0f);
            g.setColour (accent.withAlpha (0.18f));
            g.fillEllipse (badge);
            g.setColour (accent);
            g.drawEllipse (badge.reduced (1.0f), 2.0f);
            g.setFont (Theme::font (22.0f, true));
            g.drawText (glyph, badge, juce::Justification::centred);
        }
        g.setColour (alert.findColour (juce::AlertWindow::textColourId));
        textLayout.draw (g, textArea.withTrimmedLeft (glyph.isNotEmpty() ? 62 : 0).withTrimmedTop (4).toFloat());
        g.setColour (Theme::line().brighter (0.2f));
        g.drawRect (alert.getLocalBounds().toFloat(), 1.0f);
    }

    int getAlertBoxWindowFlags() override                                { return juce::ComponentPeer::windowAppearsOnTaskbar | juce::ComponentPeer::windowHasDropShadow; }
    juce::Font getAlertWindowTitleFont() override                        { return Theme::font (17.0f, true); }
    juce::Font getAlertWindowMessageFont() override                      { return Theme::font (14.0f); }
    juce::Font getAlertWindowFont() override                             { return Theme::font (14.0f); }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Theme::font (juce::jmin (14.0f, h * 0.55f), true); }
    juce::Font getPopupMenuFont() override                            { return Theme::font (14.0f); }
    juce::Font getLabelFont (juce::Label& l) override                 { return l.getFont(); }
};
