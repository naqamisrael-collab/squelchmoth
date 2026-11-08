#pragma once
// One app, one taskbar / Dock icon. Every secondary window (Welcome, Transport, Mix / Beat / Claude torn off,
// plug-in editors...) is owned by the main window, so only the main window shows on the Windows taskbar and
// the Linux task list; owned windows minimise and restore with it. On macOS the Dock is per process anyway.
#include <JuceHeader.h>

#if JUCE_WINDOWS
extern "C" void sqmOwnWindow (void* hwnd, void* ownerHwnd);   // WinWindowHelpers.cpp
#endif

/** The app's main window (set by MainWindow; null in the self-test and other headless modes). */
inline juce::Component*& sqmMainWindow()   { static juce::Component* w = nullptr; return w; }

/** Take a secondary top-level window off the taskbar and give it the main window as owner. Safe to call often. */
inline void sqmAttachToMain (juce::Component& w)
{
   #if JUCE_WINDOWS
    auto* main = sqmMainWindow();
    if (main == nullptr || main == &w) return;
    auto* peer = w.getPeer(); auto* mainPeer = main->getPeer();
    if (peer != nullptr && mainPeer != nullptr) sqmOwnWindow (peer->getNativeHandle(), mainPeer->getNativeHandle());
   #else
    juce::ignoreUnused (w);
   #endif
}

/** Attach every secondary top-level document window that is on the desktop (editors, piano rolls, the
    Channel Rack, dialogs...). Cheap: windows already attached are skipped. The main window runs it on a timer. */
inline void sqmAttachAll()
{
   #if JUCE_WINDOWS
    auto& d = juce::Desktop::getInstance();
    for (int i = d.getNumComponents(); --i >= 0;)
        if (auto* c = d.getComponent (i))
            if (c != sqmMainWindow() && dynamic_cast<juce::DocumentWindow*> (c) != nullptr) sqmAttachToMain (*c);
   #endif
}

/** Release every owned window before the main window's native window is destroyed. */
inline void sqmDetachAll()
{
   #if JUCE_WINDOWS
    auto& d = juce::Desktop::getInstance();
    for (int i = d.getNumComponents(); --i >= 0;)
        if (auto* c = d.getComponent (i))
            if (c != sqmMainWindow()) if (auto* peer = c->getPeer()) sqmOwnWindow (peer->getNativeHandle(), nullptr);
   #endif
}

/** Style flags for a secondary window: Linux hides it from the task list (Windows does it by ownership, so the
    window keeps its full title bar instead of becoming a small tool window). */
inline int sqmSecondaryStyleFlags (int flags)
{
   #if JUCE_LINUX || JUCE_BSD
    return flags & ~juce::ComponentPeer::windowAppearsOnTaskbar;
   #else
    return flags;
   #endif
}
