#pragma once
// Background VST3 / Audio Unit / VST scanner. Finds every plugin installed on the computer automatically
// (standard folders + any extra folders from Preferences > Plugins), scans only what is new or updated, and
// KEEPS what it found:
//  * the list is saved after every change - to the engine settings and to a backup file
//    ("Plug-in List.xml" next to Squelchmoth's settings) that is merged back in at every launch;
//  * a scan that is interrupted (quitting the app, cancelling) never marks a plug-in as bad - before 1.4 an
//    interrupted scan of a big shell such as Waves blacklisted it, which is why plug-ins vanished;
//  * plug-ins are only kept off the list if their scan really crashed, and they are tried again automatically
//    when their file is updated (or with Setup > Rescan All Plugins);
//  * when a session uses a plug-in that is not on the list yet, a scan runs and the plug-in loads by itself.

#include <JuceHeader.h>
#include "Prefs.h"

namespace te = tracktion;

class PluginScanner : private juce::Thread,
                      public juce::ChangeBroadcaster,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit PluginScanner (te::Engine& e) : juce::Thread ("Squelchmoth plugin scan"), engine (e)
    {
        restoreBackup();
        cleanBlacklist (false);
        engine.getPluginManager().knownPluginList.addChangeListener (this);
    }

    ~PluginScanner() override
    {
        engine.getPluginManager().knownPluginList.removeChangeListener (this);
        stopThread (60000);
        saveList();
    }

    void startScan (bool rescanAll)
    {
        if (isThreadRunning()) return;
        rescanEverything = rescanAll;
        if (rescanAll) { Prefs::set ("pl_scanCrashed", juce::String()); cleanBlacklist (true); }
        else cleanBlacklist (false);
        setStatus ("starting", 0.0f);
        startThread();
    }

    /** Runs once, on the message thread, after the current (or next) scan finishes. */
    void runAfterScan (std::function<void()> f)   { afterScan.push_back (std::move (f)); }

    void cancel()               { signalThreadShouldExit(); }
    bool isScanning() const     { return isThreadRunning(); }

    juce::String getStatus() const
    {
        const juce::ScopedLock sl (lock);
        auto n = engine.getPluginManager().knownPluginList.getNumTypes();
        return status + " (" + juce::String (n) + " third-party plugins known)";
    }

    juce::String getCurrentPlugin() const   { const juce::ScopedLock sl (lock); return currentPlugin; }
    float getProgress() const               { const juce::ScopedLock sl (lock); return progress; }
    int getNumFoundThisScan() const         { return foundThisScan.load(); }
    juce::StringArray getFailed() const     { const juce::ScopedLock sl (lock); return failed; }
    juce::Time getLastScanTime() const      { return juce::Time ((juce::int64) Prefs::getDouble ("pl_lastScan", 0)); }
    int getRestoredFromBackup() const       { return restoredFromBackup; }

    static juce::File appFolder()
    {
        if (auto* pf = Prefs::file()) return pf->getFile().getParentDirectory();
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Squelchmoth");
    }

    /** Used by the Plugins window's own list component only (the background scan scans out of process and
        does not need a dead man's pedal, which used to blacklist whatever was being scanned when you quit). */
    static juce::File deadMansPedalFile()   { return appFolder().getChildFile ("PluginScanDeadMansPedal.txt"); }
    static juce::File backupFile()          { return appFolder().getChildFile ("Plug-in List.xml"); }

    /** Folders that will be searched, per format, including the user's extra folders. */
    juce::FileSearchPath searchPathFor (juce::AudioPluginFormat& format) const
    {
        auto paths = format.getDefaultLocationsToSearch();
        for (auto& p : juce::StringArray::fromTokens (Prefs::getString ("pl_extraPaths"), ";", ""))
            if (p.trim().isNotEmpty()) paths.addIfNotAlreadyThere (juce::File (p.trim()));
        return paths;
    }

    std::function<void()> onFinished;

    /** Writes the list to the engine settings and the backup file now. */
    void saveList()
    {
        auto& list = engine.getPluginManager().knownPluginList;
        if (auto xml = list.createXml())
        {
            engine.getPropertyStorage().setXmlProperty (te::SettingID::knownPluginList64, *xml);
            if (list.getNumTypes() > 0 || ! backupFile().existsAsFile())
            {
                backupFile().getParentDirectory().createDirectory();
                xml->writeTo (backupFile());
            }
        }
    }

private:
    te::Engine& engine;
    juce::CriticalSection lock;
    juce::String status { "idle - plugins are found automatically at launch; Setup > Plugins to rescan" }, currentPlugin;
    float progress = 0.0f;
    bool rescanEverything = false;
    std::atomic<int> foundThisScan { 0 };
    juce::StringArray failed;
    int restoredFromBackup = 0;
    std::vector<std::function<void()>> afterScan;

    void setStatus (const juce::String& s, float p)
    {
        { const juce::ScopedLock sl (lock); status = s; progress = p; }
        juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override   { startTimer (1500); }   // save shortly after changes
    void timerCallback() override                                     { stopTimer(); saveList(); }

    /** Anything in the backup that the engine list lost comes back. */
    void restoreBackup()
    {
        auto f = backupFile();
        if (! f.existsAsFile()) return;
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr) return;
        juce::KnownPluginList backup;
        backup.recreateFromXml (*xml);
        auto& list = engine.getPluginManager().knownPluginList;
        for (auto& d : backup.getTypes())
        {
            bool have = false;
            for (auto& e : list.getTypes()) if (e.isDuplicateOf (d)) { have = true; break; }
            if (! have && list.addType (d)) ++restoredFromBackup;
        }
    }

    juce::StringArray crashedRecords()
    {
        auto a = juce::StringArray::fromLines (Prefs::getString ("pl_scanCrashed"));
        a.removeEmptyStrings();
        return a;
    }

    static juce::String stampFor (const juce::String& fileOrId)
    {
        juce::File f (fileOrId);
        return fileOrId + "|" + (juce::File::isAbsolutePath (fileOrId) && f.exists() ? juce::String (f.getLastModificationTime().toMilliseconds()) : juce::String ("id"));
    }

    /** Keeps a plug-in off the list only if its scan really crashed and it hasn't been updated since. */
    void cleanBlacklist (bool clearAll)
    {
        auto& list = engine.getPluginManager().knownPluginList;
        auto records = crashedRecords();
        for (auto& b : list.getBlacklistedFiles())
            if (clearAll || ! records.contains (stampFor (b)))
                list.removeFromBlacklist (b);
        deadMansPedalFile().deleteFile();
    }

    void run() override
    {
        auto& pm = engine.getPluginManager();
        auto& list = pm.knownPluginList;
        foundThisScan = 0;
        { const juce::ScopedLock sl (lock); failed.clear(); }

        if (rescanEverything)
        {
            juce::MessageManager::callAsync ([&list] { list.clear(); });
            juce::Thread::sleep (100);
        }

        juce::Array<juce::AudioPluginFormat*> formats;
        for (auto format : pm.pluginFormatManager.getFormats())
            if (format->getName() == "VST3" || format->getName() == "AudioUnit" || format->getName() == "VST")
                formats.add (format);

        juce::StringArray newlyCrashed;
        int fi = 0;
        for (auto format : formats)
        {
            if (threadShouldExit()) break;
            auto name = format->getName();
            juce::PluginDirectoryScanner scanner (list, *format, searchPathFor (*format), true, juce::File(), false);

            juce::String nextName;
            while (! threadShouldExit())
            {
                nextName = scanner.getNextPluginFileThatWillBeScanned();
                { const juce::ScopedLock sl (lock); currentPlugin = nextName; }
                auto overall = ((float) fi + scanner.getProgress()) / (float) juce::jmax (1, formats.size());
                setStatus ("scanning " + name + ": " + juce::File (nextName).getFileNameWithoutExtension() + " (" + juce::String (juce::roundToInt (overall * 100)) + "%)", overall);
                auto before = list.getNumTypes();
                auto blackBefore = list.getBlacklistedFiles();
                const bool more = scanner.scanNextFile (true, nextName);
                if (list.getNumTypes() > before) ++foundThisScan;
                if (! threadShouldExit())
                    for (auto& b : list.getBlacklistedFiles())
                        if (! blackBefore.contains (b)) newlyCrashed.add (stampFor (b));   // a real crash while scanning
                if (! more) break;
            }
            { const juce::ScopedLock sl (lock); failed.addArray (scanner.getFailedFiles()); }
            ++fi;
        }

        if (! newlyCrashed.isEmpty())
        {
            auto records = crashedRecords();
            records.addArray (newlyCrashed);
            records.removeDuplicates (false);
            juce::MessageManager::callAsync ([records] { Prefs::set ("pl_scanCrashed", records.joinIntoString ("\n")); });
        }

        juce::MessageManager::callAsync ([this] { saveList(); });
        Prefs::set ("pl_lastScan", (double) juce::Time::getCurrentTime().toMilliseconds());
        setStatus (threadShouldExit() ? "scan stopped - it continues next time" : "scan finished - " + juce::String (foundThisScan.load()) + " new plugin(s) found", 1.0f);
        juce::MessageManager::callAsync ([this]
        {
            auto jobs = std::move (afterScan);
            afterScan.clear();
            for (auto& j : jobs) if (j) j();
            if (onFinished) onFinished();
        });
    }
};
