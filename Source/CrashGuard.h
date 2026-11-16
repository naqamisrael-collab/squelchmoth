#pragma once
// Squelchmoth - crash protection.
//
//  * A small "running" marker is written at launch and removed on a clean quit, so the next launch knows if
//    Squelchmoth did not close normally - and whether it happened while a session was being opened.
//  * If the app does crash, a crash report is written (time, what was happening, the plug-ins that were running
//    on the crashing thread - from Tracktion's crash tracer - and a stack backtrace).
//  * The next launch never walks back into the same crash: a plug-in found on the crashing stack is switched off
//    (it is not loaded, but its settings stay in the session), and a crash while opening a session reopens it
//    in Safe Mode (no third-party plug-ins loaded) so the user can save their work.
//  * Safe Mode can also be chosen by holding Shift while Squelchmoth starts, like Pro Tools.
//  * Plug-ins switched off this way are listed in Setup > Plug-ins Switched Off and can be turned back on.

#include <JuceHeader.h>
#include "Prefs.h"

namespace te = tracktion;

namespace CrashGuard
{
    struct State
    {
        bool crashedLastTime = false;      // the previous run did not quit normally
        bool crashedWhileLoading = false;  // ...and it happened while a session was opening
        juce::String lastPhase;            // what the previous run was doing
        juce::StringArray culprits;        // plug-ins that were running on the crashing thread ("Name (VST3)")
        juce::File report;                 // the crash report, if one was written
        bool safeMode = false;             // third-party plug-ins are not loaded this run
    };

    inline State& state()                       { static State s; return s; }
    inline juce::File& folder()                 { static juce::File f; return f; }
    inline juce::File markerFile()              { return folder().getChildFile ("running.flag"); }
    inline juce::File reportFile()              { return folder().getChildFile ("Last crash report.txt"); }

    //==========================================================================
    // Plug-ins switched off after a crash ("Name (Format)" strings, kept in the settings file)
    inline juce::CriticalSection& listLock()    { static juce::CriticalSection c; return c; }
    inline juce::StringArray& disabledCache()   { static juce::StringArray a; return a; }

    inline juce::StringArray disabledPlugins()
    {
        const juce::ScopedLock sl (listLock());
        return disabledCache();
    }

    inline void loadDisabledList()
    {
        auto a = juce::StringArray::fromLines (Prefs::getString ("pl_disabledAfterCrash"));
        a.removeEmptyStrings();
        const juce::ScopedLock sl (listLock());
        disabledCache() = a;
    }

    inline void setPluginSwitchedOff (const juce::String& debugName, bool off)
    {
        {
            const juce::ScopedLock sl (listLock());
            if (off) disabledCache().addIfNotAlreadyThere (debugName);
            else disabledCache().removeString (debugName);
        }
        Prefs::set ("pl_disabledAfterCrash", disabledPlugins().joinIntoString ("\n"));
    }

    /** "Name (VST3)" -> the identifier Tracktion uses ("VST3-Name..."). */
    inline bool matchesIdentifier (const juce::String& debugName, const juce::String& identifier)
    {
        auto name = debugName.upToLastOccurrenceOf (" (", false, false);
        auto format = debugName.fromLastOccurrenceOf (" (", false, false).upToLastOccurrenceOf (")", false, false);
        return identifier.startsWith (format + "-" + name);
    }

    /** Called by the engine before it loads a third-party plug-in (EngineBehaviour::isPluginDisabled). */
    inline bool isPluginDisabled (const juce::String& identifier)
    {
        if (state().safeMode) return true;
        const juce::ScopedLock sl (listLock());
        for (auto& d : disabledCache()) if (matchesIdentifier (d, identifier)) return true;
        return false;
    }

    //==========================================================================
    inline void setPhase (const juce::String& phase)
    {
        if (folder() != juce::File()) markerFile().replaceWithText (phase);
    }

    inline void crashHandler (void*)
    {
        // runs inside the crash: keep it short
        juce::String r;
        r << "Squelchmoth " << JUCE_APPLICATION_VERSION_STRING << " crash report\n"
          << juce::Time::getCurrentTime().toString (true, true) << "\n"
          << "Was doing: " << markerFile().loadFileAsString() << "\n";
        auto plugins = te::CrashStackTracer::getCrashedPlugins();
        if (! plugins.isEmpty()) r << "Plug-ins running when it crashed: " << plugins.joinIntoString (" | ") << "\n";
        r << "\nBacktrace:\n" << juce::SystemStats::getStackBacktrace();
        reportFile().replaceWithText (r);
    }

    /** Call once at launch, before any session is opened. Returns what happened last time. */
    inline State& begin (const juce::File& settingsFolder, bool shiftHeld)
    {
        folder() = settingsFolder;
        folder().createDirectory();
        auto& s = state();
        loadDisabledList();

        if (markerFile().existsAsFile())
        {
            s.crashedLastTime = true;
            s.lastPhase = markerFile().loadFileAsString().trim();
            s.crashedWhileLoading = s.lastPhase.startsWith ("loading");
            if (reportFile().existsAsFile() && reportFile().getLastModificationTime() >= markerFile().getLastModificationTime() - juce::RelativeTime::seconds (5))
            {
                s.report = reportFile();
                for (auto line : juce::StringArray::fromLines (reportFile().loadFileAsString()))
                    if (line.startsWith ("Plug-ins running when it crashed: "))
                        for (auto p : juce::StringArray::fromTokens (line.fromFirstOccurrenceOf (": ", false, false), "|", ""))
                            if (p.trim().isNotEmpty()) s.culprits.addIfNotAlreadyThere (p.trim());
            }
            // never walk back into the same crash
            for (auto& c : s.culprits) setPluginSwitchedOff (c, true);
            if (s.crashedWhileLoading && s.culprits.isEmpty()) s.safeMode = true;
        }
        if (shiftHeld) s.safeMode = true;

        setPhase ("starting");
        juce::SystemStats::setApplicationCrashHandler (crashHandler);
        return s;
    }

    /** Clean quit: the next launch is normal. */
    inline void end()
    {
        if (folder() != juce::File()) markerFile().deleteFile();
    }

    /** Text for the dialog shown after a crash (empty when nothing needs saying). */
    inline juce::String explanation()
    {
        auto& s = state();
        juce::String t;
        if (s.crashedLastTime)
        {
            t << "Squelchmoth did not close normally last time";
            if (s.crashedWhileLoading) t << ", while it was opening a session";
            t << ".\n\n";
            if (! s.culprits.isEmpty())
                t << "It happened inside the plug-in " << s.culprits.joinIntoString (", ") << ". That plug-in is now switched off "
                     "(its settings are kept) so it cannot crash Squelchmoth again. Turn it back on in Setup > Plug-ins Switched Off "
                     "after updating it from its maker.\n\n";
            else if (s.safeMode)
                t << "Your session was opened in Safe Mode: third-party plug-ins are not loaded (their settings are kept). Save your work, "
                     "then choose Setup > Leave Safe Mode to load them again.\n\n";
            t << "Squelchmoth auto-saves every minute, so at most the last minute of changes can be missing.";
            if (s.report.existsAsFile()) t << "\n\nA crash report was saved: " << s.report.getFullPathName();
        }
        else if (s.safeMode)
            t << "Safe Mode (Shift was held at launch): third-party plug-ins are not loaded this time; their settings are kept. "
                 "Choose Setup > Leave Safe Mode to load them.";
        return t;
    }
}
