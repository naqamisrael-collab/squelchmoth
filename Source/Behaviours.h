#pragma once
// Squelchmoth - engine/UI behaviours and plugin windows.
// Plugin window code adapted from Tracktion Engine's examples (GPLv3).

#include "Theme.h"
#include "Signature.h"
#include "SqLocked.h"
#include "PluginSkins.h"
#include "CrashGuard.h"
#include "SqOwnedWindow.h"

//==============================================================================
/** Generic editor for plugins without their own GUI (the built-ins): one slider per parameter. */
class GenericPluginEditor : public te::Plugin::EditorComponent,
                            private juce::Timer
{
public:
    explicit GenericPluginEditor (te::Plugin& p) : plugin (p), sig (Signature::findForPlugin (p))
    {
        for (auto param : plugin.getAutomatableParameters())
        {
            auto row = std::make_unique<Row> (*param);
            content.addAndMakeVisible (*row);
            rows.push_back (std::move (row));
        }
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        if (sig != nullptr && ! sig->presets.empty())
        {
            presetBox.addItem ("Factory presets...", 1);
            for (int i = 0; i < (int) sig->presets.size(); ++i) presetBox.addItem (sig->presets[(size_t) i].name, i + 2);
            presetBox.setSelectedId (1, juce::dontSendNotification);
            presetBox.onChange = [this]
            {
                auto idx = presetBox.getSelectedId() - 2;
                if (idx >= 0 && idx < (int) sig->presets.size()) { Signature::applyPreset (plugin, sig->presets[(size_t) idx]); for (auto& r : rows) r->refresh(); }
            };
            addAndMakeVisible (presetBox);
        }
        bypass.setClickingTogglesState (true);
        bypass.setToggleState (! plugin.isEnabled(), juce::dontSendNotification);
        bypass.setColour (juce::TextButton::buttonOnColourId, Theme::amber().darker (0.3f));
        bypass.onClick = [this] { plugin.setEnabled (! bypass.getToggleState()); };
        addAndMakeVisible (bypass);

        setSize (460, juce::jlimit (160, 680, (int) rows.size() * 34 + 16 + headerHeight));
        startTimerHz (10);
    }

    bool allowWindowResizing() override                         { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    static constexpr int headerHeight = 58;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        auto header = getLocalBounds().removeFromTop (headerHeight);
        auto accent = sig != nullptr ? juce::Colour (sig->colour) : Theme::violet();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.35f), 0, 0, Theme::panel2(), (float) getWidth(), (float) headerHeight, false));
        g.fillRect (header);
        g.setColour (accent);
        g.fillRect (header.removeFromLeft (6));
        g.setColour (Theme::text());
        g.setFont (Theme::font (18.0f, true));
        g.drawText (Signature::displayName (plugin), 16, 6, getWidth() - 220, 24, juce::Justification::centredLeft);
        g.setColour (Theme::dim());
        g.setFont (Theme::font (11.0f));
        juce::String sub = sig != nullptr ? juce::String (sig->tagline) : plugin.getName();
        if (sig != nullptr) sub << "   |   " << sig->category;
        g.drawFittedText (sub, 16, 30, getWidth() - 32, 24, juce::Justification::topLeft, 2);
        g.setColour (Theme::line());
        g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (headerHeight).reduced (10, 6);
        bypass.setBounds (header.removeFromRight (70).removeFromTop (22));
        header.removeFromRight (6);
        if (presetBox.isVisible()) presetBox.setBounds (header.removeFromRight (150).removeFromTop (22));
        viewport.setBounds (r);
        content.setSize (getWidth() - 10, (int) rows.size() * 34 + 16);
        int y = 8;
        for (auto& rw : rows) { rw->setBounds (8, y, content.getWidth() - 16, 30); y += 34; }
    }

private:
    struct Row : public juce::Component
    {
        explicit Row (te::AutomatableParameter& p) : param (p)
        {
            label.setText (p.getParameterName(), juce::dontSendNotification);
            label.setColour (juce::Label::textColourId, Theme::text());
            addAndMakeVisible (label);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 22);
            slider.setRange (0.0, 1.0);
            slider.textFromValueFunction = [this] (double v) { return param.valueToString (juce::jmap ((float) v, param.getValueRange().getStart(), param.getValueRange().getEnd())); };
            slider.onValueChange = [this] { param.setNormalisedParameter ((float) slider.getValue(), juce::sendNotification); };
            slider.onDragStart = [this] { param.parameterChangeGestureBegin(); };
            slider.onDragEnd = [this] { param.parameterChangeGestureEnd(); };
            addAndMakeVisible (slider);
            refresh();
        }
        void refresh()  { if (! slider.isMouseButtonDown()) slider.setValue (param.getCurrentNormalisedValue(), juce::dontSendNotification); }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromLeft (130));
            slider.setBounds (r);
        }
        te::AutomatableParameter& param;
        juce::Label label;
        juce::Slider slider;
    };

    void timerCallback() override   { for (auto& r : rows) r->refresh(); bypass.setToggleState (! plugin.isEnabled(), juce::dontSendNotification); }

    te::Plugin& plugin;
    const Signature::Entry* sig = nullptr;
    juce::ComboBox presetBox;
    juce::TextButton bypass { "BYPASS" };
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
};

//==============================================================================
#if JUCE_MAC
extern "C" void sqmMakeFloatingPanel (void* nsViewHandle, bool floating);
extern "C" void sqmActivateApp();
#endif

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& plug)
        : DocumentWindow (Signature::displayName (plug), Theme::bg(), DocumentWindow::closeButton, true),
          plugin (plug)
    {
        getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
        setResizeLimits (100, 50, 4000, 4000);
        recreateEditor();
        setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());
        setUsingNativeTitleBar (true);
       #if JUCE_LINUX
        setAlwaysOnTop (true);
       #endif
        updateStoredBounds = true;
    }

    ~PluginWindow() override
    {
        updateStoredBounds = false;
        plugin.edit.flushPluginStateIfNeeded (plugin);
        setEditor (nullptr);
    }

    static std::unique_ptr<juce::Component> create (te::Plugin& plugin)
    {
        if (auto ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
            if (ext->getAudioPluginInstance() == nullptr)
                return {};

        auto w = std::make_unique<PluginWindow> (plugin);
        if (w->editor == nullptr) return {};
        sqmAttachToMain (*w);
        w->setVisible (true);
       #if JUCE_MAC
        // Plug-in windows float above the Edit/Mix window like Pro Tools (and hide when the app is in the background)
        if (auto* peer = w->getPeer()) sqmMakeFloatingPanel (peer->getNativeHandle(), true);
       #endif
        w->toFront (true);
        return w;
    }

    void recreateEditor()
    {
        setEditor (nullptr);
        auto ed = plugin.createEditor();
        if (ed == nullptr)
        {
            if (Signature::findForPlugin (plugin) != nullptr) ed = std::make_unique<Skins::SkinEditor> (plugin);
            else ed = std::make_unique<GenericPluginEditor> (plugin);
        }
        setEditor (std::move (ed));
    }

    void recreateEditorAsync()
    {
        setEditor (nullptr);
        juce::Timer::callAfterDelay (50, [sp = SafePointer<PluginWindow> (this)] { if (sp != nullptr) sp->recreateEditor(); });
    }

private:
    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
    {
        JUCE_AUTORELEASEPOOL
        {
            setConstrainer (nullptr);
            editor.reset();
            if (newEditor != nullptr)
            {
                editor = std::move (newEditor);
                setContentNonOwned (editor.get(), true);
            }
            setResizable (editor == nullptr || editor->allowWindowResizing(), false);
            if (editor != nullptr && editor->allowWindowResizing())
                setConstrainer (editor->getBoundsConstrainer());
        }
    }

    void moved() override
    {
        if (updateStoredBounds)
        {
            plugin.windowState->lastWindowBounds = getBounds();
            plugin.edit.pluginChanged (plugin);
        }
    }

    void closeButtonPressed() override  { plugin.windowState->closeWindowExplicitly(); }
    float getDesktopScaleFactor() const override { return 1.0f; }

    te::Plugin& plugin;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
    bool updateStoredBounds = false;
};

//==============================================================================
class SquelchmothUIBehaviour : public te::UIBehaviour
{
public:
    std::function<te::Edit*()> getEdit;
    std::function<void (const juce::String&)> onMessage;
    bool headless = false;

    te::Edit* getCurrentlyFocusedEdit() override    { return getEdit ? getEdit() : nullptr; }
    te::Edit* getLastFocusedEdit() override         { return getCurrentlyFocusedEdit(); }

    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (headless) return {};
        if (auto ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
            return PluginWindow::create (ws->plugin);
        return {};
    }

    void recreatePluginWindowContentAsync (te::Plugin& p) override
    {
        if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
            return w->recreateEditorAsync();
        UIBehaviour::recreatePluginWindowContentAsync (p);
    }

    /** Renders (bounces, freezes, analysis) run synchronously; Claude waits for the result. */
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& job) override
    {
        // Renders run here on the calling thread. Some sources (a clip that follows tempo from its file name or loop
        // info, a freeze, a proxy) are prepared by jobs that post back to the message thread, so the message loop has to
        // turn while the render waits for them - as a modal progress window would - or the render waits forever.
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        const bool onMessageThread = mm != nullptr && mm->isThisTheMessageThread();
        // Only a stalled render pumps (progress unchanged for 100 ms); a render that is moving never lets other events in.
        float lastProgress = -1.0f; auto lastMove = juce::Time::getMillisecondCounterHiRes();
        while (job.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
            if (! onMessageThread) continue;
            const float pr = job.getCurrentTaskProgress(); const auto now = juce::Time::getMillisecondCounterHiRes();
            if (pr != lastProgress) { lastProgress = pr; lastMove = now; continue; }
            if (now - lastMove > 100.0) mm->runDispatchLoopUntil (5);
        }
    }

    void showWarningAlert (const juce::String& title, const juce::String& message) override
    {
        if (onMessage) onMessage (title + ": " + message);
        if (! headless)
            Ui::message (juce::MessageBoxIconType::WarningIcon, title, message);
    }

    void showInfoMessage (const juce::String& m) override       { if (onMessage) onMessage (m); }
    void showWarningMessage (const juce::String& m) override    { if (onMessage) onMessage ("Warning: " + m); }
};

//==============================================================================
class SquelchmothEngineBehaviour : public te::EngineBehaviour
{
public:
   #if JUCE_MAC || JUCE_WINDOWS
    bool canScanPluginsOutOfProcess() override                       { return true; }
   #endif
    bool autoInitialiseDeviceManager() override                      { return true; }
    juce::String getDefaultAudioRecordingFilePattern() override      { return "%projectdir%/Audio Files/%edit%_%track%_Take_%take%"; }
    bool autoAddClipEdgeFades() override                             { return true; }

    // Playback Engine > Host Engine (read live, so changes apply without restarting)
    int getNumberOfCPUsToUseForAudio() override                      { return Prefs::hostProcessors(); }
    // Crash protection: plug-ins switched off after a crash (and every third-party plug-in in Safe Mode) are not loaded
    bool isPluginDisabled (const juce::String& identifier) override  { return CrashGuard::isPluginDisabled (identifier); }
    // Dynamic Plug-in Processing lives in te::PluginNode (see Performance.h). Bypassed plug-ins stay in the graph so
    // bypassing never interrupts playback or changes delay compensation; they cost no CPU while bypassed.
    bool enableReadAheadForTimeStretchNodes() override               { return Prefs::elasticBackground(); }

    // Editions (1.9): every built-in plug-in the engine creates passes this gate; a type this copy doesn't own loads
    // as the Locked placeholder, which keeps its settings and shows how to unlock it.
    bool isPluginTypeAllowed (const juce::String& type) override
    {
        if (Edition::productTypes().isEmpty()) for (auto& e : Signature::entries()) Edition::productTypes().addIfNotAlreadyThere (e.type);
        return Edition::isTypeAllowed (type);
    }
    te::Plugin::Ptr createPlaceholderPlugin (te::PluginCreationInfo info) override   { return new SqLockedPlugin (info); }
};
