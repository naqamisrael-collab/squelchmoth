#pragma once
// The Locked placeholder (1.9 "Store"). When a session, preset, menu or Claude asks for a plug-in this copy does not own,
// the engine builds this instead of the real thing (EngineBehaviour::createPlaceholderPlugin). It:
//  - keeps the plug-in's saved state exactly as it was (type, parameters, automation, zones...), so saving the session
//    loses nothing and the real plug-in comes back with every setting once it is unlocked and the session reopened;
//  - passes audio through untouched (effects) or stays silent (instruments);
//  - opens a designed "Locked" panel with what it is, which product it belongs to, and Buy / Enter Key.
#include "Signature.h"
#include "Edition.h"
#include "Theme.h"

class SqLockedPlugin : public te::Plugin
{
public:
    static constexpr const char* xmlTypeName = "sqmLocked";
    explicit SqLockedPlugin (te::PluginCreationInfo info) : te::Plugin (info), originalType (info.state[te::IDs::type].toString())
    {
        for (auto& e : Signature::entries()) if (originalType == e.type) { entry = &e; break; }
        instrument = entry != nullptr && juce::String (entry->category) == "Instruments";
    }
    ~SqLockedPlugin() override   { notifyListenersOfDeletion(); }

    juce::String getName() const override               { return productName() + " (locked)"; }
    juce::String getPluginType() override               { return originalType; }   // saved back exactly as it was
    juce::String getShortName (int) override            { return "Locked"; }
    juce::String getSelectableDescription() override    { return getName(); }
    bool isSynth() override                             { return instrument; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return ! instrument; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 0.0; }
    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (instrument && fc.destBuffer != nullptr) fc.destBuffer->clear (fc.bufferStartSample, fc.bufferNumSamples);   // a locked instrument is silent
        if (fc.bufferForMidiMessages != nullptr && instrument) fc.bufferForMidiMessages->clear();
    }
    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}   // nothing to restore: the state stays untouched

    juce::String productName() const    { return entry != nullptr ? juce::String (entry->name) : originalType.replace ("airwindows_", "AW "); }
    juce::String tagline() const        { return entry != nullptr ? juce::String (entry->tagline) : juce::String ("From the AirWindows collection"); }
    juce::String artKey() const         { return entry != nullptr ? juce::String (entry->art) : juce::String(); }
    juce::uint32 colour() const         { return entry != nullptr ? entry->colour : 0xff8f84aa; }
    const juce::String originalType;

    std::unique_ptr<EditorComponent> createEditor() override;

private:
    const Signature::Entry* entry = nullptr;
    bool instrument = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqLockedPlugin)
};

/** The Locked panel: the plug-in's own plate art behind glass, its name and job, the product that unlocks it with the
    price, and two buttons. Sized like a small plug-in window. */
class SqLockedEditor : public te::Plugin::EditorComponent
{
public:
    explicit SqLockedEditor (SqLockedPlugin& p) : plugin (p)
    {
        int size = 0;
        if (auto key = p.artKey(); key.isNotEmpty())
            if (auto data = BinaryData::getNamedResource (("pa_" + key + "_jpg").toRawUTF8(), size)) art = juce::ImageFileFormat::loadFrom (data, (size_t) size);
        product = Edition::productForType (p.originalType);
        buy.setButtonText ("GET " + juce::String (product->name).toUpperCase() + "  " + juce::String (product->price));
        buy.onClick = [this] { juce::URL (Edition::storeUrl (product->code)).launchInDefaultBrowser(); };
        key.setButtonText ("ENTER KEY...");
        key.onClick = [this] { if (Edition::showUnlock()) Edition::showUnlock() (product->code); };
        for (auto* b : { &buy, &key }) { b->setColour (juce::TextButton::buttonColourId, juce::Colour (p.colour()).withAlpha (0.85f)); b->setColour (juce::TextButton::textColourOffId, juce::Colours::black); addAndMakeVisible (b); }
        key.setColour (juce::TextButton::buttonColourId, juce::Colour (0x33ffffff)); key.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setSize (640, 400);
    }
    bool allowWindowResizing() override                         { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override   { return nullptr; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.fillAll (juce::Colour (0xff0d0a16));
        if (art.isValid())
        {
            g.setOpacity (0.55f);
            g.drawImage (art, r, juce::RectanglePlacement::fillDestination);
            g.setOpacity (1.0f);
        }
        const auto accent = juce::Colour (plugin.colour());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xe60d0a16), 0, r.getBottom(), juce::Colour (0x660d0a16), 0, r.getY(), false));
        g.fillRect (r);
        // glass card
        auto card = r.reduced (36, 40);
        g.setColour (juce::Colour (0xcc120e1e)); g.fillRoundedRectangle (card, 14.0f);
        g.setColour (accent.withAlpha (0.8f)); g.drawRoundedRectangle (card, 14.0f, 1.5f);
        auto c = card.reduced (28, 22);
        // lock glyph
        auto lock = c.removeFromTop (54).withSizeKeepingCentre (40, 50);
        juce::Path shackle; shackle.addCentredArc (lock.getCentreX(), lock.getY() + 20, 13, 13, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.setColour (accent); g.strokePath (shackle, juce::PathStrokeType (4.0f));
        g.fillRoundedRectangle (juce::Rectangle<float> (lock.getX() + 2, lock.getY() + 20, 36, 28), 5.0f);
        g.setColour (juce::Colour (0xff0d0a16)); g.fillEllipse (lock.getCentreX() - 4, lock.getY() + 29, 8, 8);
        c.removeFromTop (8);
        g.setColour (juce::Colours::white); g.setFont (Theme::font (26.0f, true));
        g.drawText (plugin.productName().toUpperCase(), c.removeFromTop (34), juce::Justification::centred);
        g.setColour (juce::Colour (0xffcfc6e8)); g.setFont (Theme::font (13.0f));
        g.drawFittedText (plugin.tagline(), c.removeFromTop (40).toNearestInt(), juce::Justification::centred, 2);
        c.removeFromTop (6);
        g.setColour (accent); g.setFont (Theme::font (13.5f, true));
        g.drawText (Edition::lockReason (plugin.originalType), c.removeFromTop (22), juce::Justification::centred);
        g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (11.5f));
        g.drawFittedText ("Your settings are kept in the session. Unlock it and reopen the session to hear them.", c.removeFromTop (34).toNearestInt(), juce::Justification::centred, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (36, 40).reduced (28, 22);
        auto row = r.removeFromBottom (40);
        const int w = juce::jmin (260, (row.getWidth() - 12) / 2);
        auto both = row.withSizeKeepingCentre (w * 2 + 12, 40);
        buy.setBounds (both.removeFromLeft (w)); both.removeFromLeft (12); key.setBounds (both);
    }

private:
    SqLockedPlugin& plugin; const Edition::Product* product = nullptr; juce::Image art;
    juce::TextButton buy, key;
};

inline std::unique_ptr<te::Plugin::EditorComponent> SqLockedPlugin::createEditor()   { return std::make_unique<SqLockedEditor> (*this); }
