#pragma once
// Squelchmoth 1.52 - the Capture window: measure a unit and keep a model of it without typing a word to the assistant.
// Every button goes through the same `capture` tool the assistant uses. Three steps down the left (make the sweep, fit
// the recording, use a capture on a track); on the right the chosen capture's recall sheet - notes on how the unit was
// set, and a photo of its front panel - kept with the capture.

#include <JuceHeader.h>
#include "CaptureStore.h"

class CaptureContent : public juce::Component
{
public:
    std::function<juce::String (const juce::var&)> run;        // the capture tool: arguments in, its answer (or its complaint) out
    std::function<juce::StringArray()> trackNames;             // the session's audio tracks

    CaptureContent()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &levelBox, &sweepBtn, &trackBox, &nameEd, &fitBtn, &captureBox, &useBox, &useBtn, &removeBtn, &notesEd, &saveBtn, &photoBtn, &status }) addAndMakeVisible (c);
        int id = 1; for (int db : { 0, -3, -6, -12, -18 }) levelBox.addItem (juce::String (db) + " dBFS", id++); levelBox.setSelectedId (3, juce::dontSendNotification);
        levelBox.setTooltip ("How loud the sweep peaks. Sweep at the hottest level you will use the unit at.");
        sweepBtn.setButtonText ("1  Add the sweep track"); fitBtn.setButtonText ("2  Fit the recording"); useBtn.setButtonText ("3  Use on this track"); removeBtn.setButtonText ("Remove"); saveBtn.setButtonText ("Save notes"); photoBtn.setButtonText ("Photo...");
        nameEd.setTextToShowWhenEmpty ("Name the capture (e.g. Valve Preamp, gain at 3 o'clock)", juce::Colours::grey); nameEd.setFont (Theme::font (13.0f)); nameEd.onReturnKey = [this] { fitNow(); };
        notesEd.setMultiLine (true); notesEd.setReturnKeyStartsNewLine (true); notesEd.setFont (Theme::font (12.5f)); notesEd.setTextToShowWhenEmpty ("How was the unit set? Knob positions, switches, cables, anything needed to set it up again.", juce::Colours::grey);
        status.setMultiLine (true); status.setReadOnly (true); status.setCaretVisible (false); status.setFont (Theme::font (12.0f)); status.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff101116));
        sweepBtn.onClick = [this] { addSweep(); }; fitBtn.onClick = [this] { fitNow(); }; useBtn.onClick = [this] { use (useBox.getText(), captureBox.getText()); };
        removeBtn.onClick = [this] { const auto n = captureBox.getText(); if (n.isNotEmpty()) { say (ask ("remove", {}, n)); refresh(); } };
        saveBtn.onClick = [this] { setNotes (captureBox.getText(), notesEd.getText()); };
        photoBtn.onClick = [this]
        {
            const auto n = captureBox.getText(); if (n.isEmpty()) { say ("Choose a capture first."); return; }
            chooser = std::make_unique<juce::FileChooser> ("A photo of the unit's front panel", juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.jpg;*.jpeg;*.png");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, n] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) setPhoto (n, f); });
        };
        captureBox.onChange = [this] { showSheet(); };
        setSize (860, 560); setWantsKeyboardFocus (true);
    }

    /** Fills the pickers again from the session and the store. */
    void refresh()
    {
        const auto keepT = trackBox.getText(), keepU = useBox.getText(), keepC = captureBox.getText(); trackBox.clear (juce::dontSendNotification); useBox.clear (juce::dontSendNotification); captureBox.clear (juce::dontSendNotification);
        const auto tracks = trackNames ? trackNames() : juce::StringArray(); int id = 1; for (auto& t : tracks) { trackBox.addItem (t, id); useBox.addItem (t, id); ++id; }
        id = 1; for (auto& n : CaptureStore::list()) captureBox.addItem (n, id++);
        auto pick = [] (juce::ComboBox& b, const juce::String& want, const juce::String& fallback) { for (int i = 0; i < b.getNumItems(); ++i) if (b.getItemText (i) == want) { b.setSelectedItemIndex (i, juce::dontSendNotification); return; } for (int i = 0; i < b.getNumItems(); ++i) if (b.getItemText (i) == fallback) { b.setSelectedItemIndex (i, juce::dontSendNotification); return; } if (b.getNumItems() > 0) b.setSelectedItemIndex (0, juce::dontSendNotification); };
        pick (trackBox, keepT, "Capture Sweep"); pick (useBox, keepU, {}); pick (captureBox, keepC, {}); showSheet();
    }
    // the buttons' work, also called by the self-test
    void addSweep() { juce::var a (new juce::DynamicObject()); a.getDynamicObject()->setProperty ("action", "sweep"); a.getDynamicObject()->setProperty ("level_db", levelBox.getText().getDoubleValue()); say (run ? run (a) : juce::String()); refresh(); for (int i = 0; i < trackBox.getNumItems(); ++i) if (trackBox.getItemText (i) == "Capture Sweep") trackBox.setSelectedItemIndex (i, juce::dontSendNotification); }
    void fit (const juce::String& track, const juce::String& name) { say (ask ("fit", track, name)); refresh(); for (int i = 0; i < captureBox.getNumItems(); ++i) if (captureBox.getItemText (i).equalsIgnoreCase (name.trim())) captureBox.setSelectedItemIndex (i, juce::dontSendNotification); showSheet(); }
    void use (const juce::String& track, const juce::String& name) { if (name.isEmpty()) { say ("Choose a capture first."); return; } say (ask ("use", track, name)); }
    void setNotes (const juce::String& name, const juce::String& notes) { if (name.isEmpty()) { say ("Choose a capture first."); return; } juce::var a (new juce::DynamicObject()); a.getDynamicObject()->setProperty ("action", "recall"); a.getDynamicObject()->setProperty ("name", name); a.getDynamicObject()->setProperty ("notes", notes); say (run ? run (a) : juce::String()); showSheet(); }
    void setPhoto (const juce::String& name, const juce::File& f) { juce::var a (new juce::DynamicObject()); a.getDynamicObject()->setProperty ("action", "recall"); a.getDynamicObject()->setProperty ("name", name); a.getDynamicObject()->setProperty ("photo", f.getFullPathName()); say (run ? run (a) : juce::String()); showSheet(); }
    void choose (const juce::String& name) { for (int i = 0; i < captureBox.getNumItems(); ++i) if (captureBox.getItemText (i) == name) captureBox.setSelectedItemIndex (i, juce::dontSendNotification); showSheet(); }
    juce::String message() const { return status.getText(); }
    juce::String chosen() const { return captureBox.getText(); }
    juce::String notesShown() const { return notesEd.getText(); }
    bool photoShown() const { return photo.isValid(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff15161b)); g.setColour (Theme::text()); g.setFont (Theme::font (15.0f)); g.drawText ("Capture - measure a unit, keep a model of it", 16, 10, getWidth() - 32, 22, juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (11.5f));
        g.drawFittedText ("1  Add the sweep, play that track through the unit and record it back onto another track from 0 s (or put plug-ins on the sweep track).   2  Pick the track that holds the result, name it, fit.   3  Put it on a track - Voltage Capture plays it.",
                          16, 34, getWidth() - 32, 30, juce::Justification::topLeft, 2);
        g.drawText ("Sweep level", labelAt (levelBox), juce::Justification::centredLeft); g.drawText ("Recording on", labelAt (trackBox), juce::Justification::centredLeft); g.drawText ("Capture", labelAt (captureBox), juce::Justification::centredLeft); g.drawText ("Track", labelAt (useBox), juce::Justification::centredLeft);
        g.setColour (Theme::text()); g.setFont (Theme::font (12.5f)); g.drawText ("Recall sheet" + (captureBox.getText().isNotEmpty() ? " - " + captureBox.getText() : juce::String()), sheetTitle, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff0e0f13)); g.fillRoundedRectangle (photoArea.toFloat(), 4.0f);
        if (photo.isValid()) g.drawImage (photo, photoArea.toFloat().reduced (3.0f), juce::RectanglePlacement::centred);
        else { g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f)); g.drawFittedText ("No photo yet - add one of the unit's front panel so its knobs can be set the same way again.", photoArea.reduced (12), juce::Justification::centred, 3); }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16); r.removeFromTop (54); auto left = r.removeFromLeft (juce::jmin (380, r.getWidth() / 2)); r.removeFromLeft (16);
        auto row = [&] (juce::Component& a, juce::Component* b, int labelW) { auto line = left.removeFromTop (28); line.removeFromLeft (labelW); if (b != nullptr) { b->setBounds (line.removeFromRight (150)); line.removeFromRight (8); } a.setBounds (line); left.removeFromTop (8); };
        row (levelBox, &sweepBtn, 90); left.removeFromTop (6); row (trackBox, nullptr, 90); row (nameEd, &fitBtn, 0); left.removeFromTop (6); row (captureBox, &removeBtn, 90); row (useBox, &useBtn, 90);
        status.setBounds (left);
        sheetTitle = r.removeFromTop (22); r.removeFromTop (4); auto buttons = r.removeFromBottom (28); saveBtn.setBounds (buttons.removeFromLeft (110)); buttons.removeFromLeft (8); photoBtn.setBounds (buttons.removeFromLeft (90));
        r.removeFromBottom (8); photoArea = r.removeFromBottom (juce::jmax (120, r.getHeight() / 2)); r.removeFromBottom (8); notesEd.setBounds (r);
    }
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress ('1')) { addSweep(); return true; } if (k == juce::KeyPress ('2')) { fitNow(); return true; } if (k == juce::KeyPress ('3')) { use (useBox.getText(), captureBox.getText()); return true; } return false; }

private:
    juce::Rectangle<int> labelAt (juce::Component& c) const { return { c.getX() - 90, c.getY(), 86, c.getHeight() }; }
    void fitNow() { if (nameEd.getText().trim().isEmpty()) { say ("Give the capture a name first."); nameEd.grabKeyboardFocus(); return; } fit (trackBox.getText(), nameEd.getText()); }
    juce::String ask (const juce::String& action, const juce::String& track, const juce::String& name)
    { juce::var a (new juce::DynamicObject()); a.getDynamicObject()->setProperty ("action", action); if (track.isNotEmpty()) a.getDynamicObject()->setProperty ("track", track); if (name.isNotEmpty()) a.getDynamicObject()->setProperty ("name", name); return run ? run (a) : juce::String(); }
    void say (const juce::String& s) { status.setText (s, false); }
    void showSheet() { const auto n = captureBox.getText(); notesEd.setText (n.isNotEmpty() ? CaptureStore::notes (n) : juce::String(), false); const auto f = n.isNotEmpty() ? CaptureStore::photo (n) : juce::File(); photo = f.existsAsFile() ? juce::ImageFileFormat::loadFrom (f) : juce::Image(); repaint(); }

    juce::ComboBox levelBox, trackBox, captureBox, useBox; juce::TextButton sweepBtn, fitBtn, useBtn, removeBtn, saveBtn, photoBtn; juce::TextEditor nameEd, notesEd, status;
    juce::Rectangle<int> sheetTitle, photoArea; juce::Image photo; std::unique_ptr<juce::FileChooser> chooser;
};

/** Connects the Capture window to the session and its tools (the app and the self-test both use this). */
template <typename Tools, typename Sess>
void attachCapture (CaptureContent& c, Tools& tools, Sess& session)
{
    c.trackNames = [&session] { juce::StringArray n; if (session.edit != nullptr) for (auto* t : te::getAudioTracks (*session.edit)) n.add (t->getName()); return n; };
    c.run = [&tools] (const juce::var& args) { return tools.call ("capture", args).text; };
    c.refresh();
}
