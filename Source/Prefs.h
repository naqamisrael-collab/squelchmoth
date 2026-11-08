#pragma once
// Squelchmoth - user preferences (Setup > Preferences). Every option is a named value in the app's
// settings file, read live by the code that needs it, so changes take effect immediately.

#include <JuceHeader.h>

namespace Prefs
{
    inline juce::PropertiesFile*& file()    { static juce::PropertiesFile* p = nullptr; return p; }

    inline bool   getBool   (const char* key, bool d)                 { return file() ? file()->getBoolValue (key, d) : d; }
    inline int    getInt    (const char* key, int d)                  { return file() ? file()->getIntValue (key, d) : d; }
    inline double getDouble (const char* key, double d)               { return file() ? file()->getDoubleValue (key, d) : d; }
    inline juce::String getString (const char* key, const juce::String& d = {}) { return file() ? file()->getValue (key, d) : d; }

    inline juce::ChangeBroadcaster& broadcaster()   { static juce::ChangeBroadcaster b; return b; }

    /** Settings that never leave the app: the MCP secret, licence keys, tunnel and Anthropic API keys, and the chat
        endpoint (so nothing can quietly point Squelchmoth Chat somewhere else). set_preference refuses them. */
    inline bool isPrivateKey (const juce::String& key)
    {
        return key == "mcpToken" || key.startsWith ("licenceKey") || key == "cl_ngrokToken" || key == "cl_anthropicKey" || key == "chat_apiBase";
    }

    inline void set (const char* key, const juce::var& v)
    {
        if (file() == nullptr) return;
        file()->setValue (key, v);
        file()->saveIfNeeded();
        broadcaster().sendChangeMessage();
    }

    //==========================================================================
    // Playback Engine - Host Engine (Pro Tools names) and Squelchmoth's overload protection
    inline int    numCpus()                 { return juce::jmax (1, juce::SystemStats::getNumCpus()); }
    inline int    hostProcessors()          { auto v = getInt ("pe_hostProcessors", 0); return v <= 0 ? numCpus() : juce::jlimit (1, numCpus(), v); }   // 0 = all
    inline int    cpuUsageLimitPercent()    { return juce::jlimit (40, 99, getInt ("pe_cpuLimit", 95)); }
    inline bool   ignoreErrors()            { return getBool ("pe_ignoreErrors", false); }
    inline bool   dynamicPluginProcessing() { return getBool ("pe_dynamicPlugins", true); }
    inline bool   lowBufferOptimisation()   { return getBool ("pe_lowBufferOpt", true); }
    inline bool   overloadProtection()      { return getBool ("pe_overloadProtection", true); }
    inline bool   elasticBackground()       { return getBool ("pe_elasticBackground", false); }
    inline int    diskCacheSeconds()        { return juce::jlimit (1, 60, getInt ("pe_diskCacheSeconds", 6)); }

    //==========================================================================
    // Operation
    inline bool   playFromSelection()       { return getBool ("op_playFromSelection", true); }      // play starts at the Edit selection start
    // Pro Tools "Timeline Insertion / Play Start Marker Follows Playback" is OFF by default: Stop returns to where play started
    inline bool   returnToStartOnStop()     { return getBool ("op_returnToStart", true); }
    inline bool   insertionFollowsPlayback(){ return ! returnToStartOnStop(); }
    // Edit window scrolling during playback: 0 = No Auto-Scrolling, 1 = Page, 2 = Continuous (playhead centred)
    inline int    scrollMode()              { return juce::jlimit (0, 2, getInt ("op_scrollMode", getBool ("op_followPlayhead", true) ? 1 : 0)); }
    inline bool   followPlayhead()          { return scrollMode() != 0; }
    inline bool   linkTimelineAndEdit()     { return getBool ("op_linkTimelineEdit", true); }       // Edit selection drives the loop/playback range
    inline bool   numericKeypadTransport()  { return getBool ("op_numericKeypad", true); }
    inline int    autosaveSeconds()         { return getInt  ("op_autosaveSeconds", 60); }    // (pre-1.7 key, kept for old settings files)
    // Auto-Save and Auto Backup (Pro Tools: Preferences > Operation > Auto Backup)
    inline bool   autoSaveSession()         { return getBool ("op_autoSave", true); }
    inline int    autoSaveMinutes()         { return juce::jlimit (1, 99, getInt ("op_autoSaveMinutes", 2)); }
    inline bool   autoBackup()              { return getBool ("op_autoBackup", true); }
    inline int    autoBackupMinutes()       { return juce::jlimit (1, 99, getInt ("op_autoBackupMinutes", 3)); }
    inline int    autoBackupKeep()          { return juce::jlimit (1, 999, getInt ("op_autoBackupKeep", 20)); }
    inline bool   autoBackupMirror()        { return getBool ("op_autoBackupMirror", true); }   // second copy on the system drive
    inline double preRollSeconds()          { return getDouble ("op_preRoll", 2.0); }
    inline double postRollSeconds()         { return getDouble ("op_postRoll", 2.0); }
    inline bool   preRollEnabled()          { return getBool ("op_preRollOn", false); }
    inline bool   postRollEnabled()         { return getBool ("op_postRollOn", false); }
    inline int    countOffBars()            { return getInt  ("op_countOffBars", 0); }               // 0 = off, 1 or 2 bars
    inline bool   quickPunch()              { return getBool ("op_quickPunch", false); }
    inline bool   loopRecord()              { return getBool ("op_loopRecord", false); }
    inline bool   waitForNote()             { return getBool ("op_waitForNote", false); }
    inline bool   midiMerge()               { return getBool ("op_midiMerge", false); }
    inline bool   solidPlayhead()           { return getBool ("op_solidPlayhead", true); }
    inline bool   scrollToTrackOnSelect()   { return getBool ("op_scrollToTrack", true); }
    inline bool   showWelcome()             { return getBool ("op_showWelcome", true); }

    // Editing
    inline double defaultFadeIn()           { return getDouble ("ed_fadeIn", 0.01); }
    inline double defaultFadeOut()          { return getDouble ("ed_fadeOut", 0.01); }
    inline double fadeCommandLength()       { return getDouble ("ed_fadeCmd", 0.25); }              // Cmd+F fade length when no selection edge
    inline juce::String fadeShape()         { return getString ("ed_fadeShape", "linear"); }
    inline bool   autoFadesOnEdits()        { return getBool ("ed_autoFades", true); }                // tiny fades on every separate / trim to avoid clicks
    inline double stripSilenceThreshold()   { return getDouble ("ed_stripThresholdDb", -48.0); }
    inline double stripSilenceMinLength()   { return getDouble ("ed_stripMinLen", 0.25); }
    inline double stripSilencePad()         { return getDouble ("ed_stripPad", 0.02); }
    inline bool   clipNamesVisible()        { return getBool ("ed_clipNames", true); }
    inline bool   clipGainLineVisible()     { return getBool ("ed_clipGainLine", true); }
    inline bool   zoomTogglesFollowSelection() { return getBool ("ed_zoomToggle", true); }
    inline bool   separateCopiesGain()      { return getBool ("ed_separateGain", true); }
    inline bool   edgeTrimOnlyNearEdges()   { return getBool ("ed_edgeTrim", true); }
    inline int    defaultTrackHeight()      { return getInt ("ed_trackHeight", 84); }
    inline bool   recordingToNewPlaylist()  { return getBool ("ed_recordNewPlaylist", false); }

    // Mixing
    inline double newTrackFaderDb()         { return getDouble ("mx_newTrackDb", 0.0); }
    inline double newSendDb()               { return getDouble ("mx_newSendDb", -10.0); }
    inline bool   sendsPostFader()          { return getBool ("mx_sendsPost", true); }
    inline int    meterHoldMs()             { return getInt ("mx_meterHold", 1500); }
    inline double meterDecayDbPerSec()      { return getDouble ("mx_meterDecay", 36.0); }
    inline bool   soloLatching()            { return getBool ("mx_soloLatch", true); }
    inline bool   groupsActive()            { return getBool ("mx_groupsActive", true); }
    inline bool   narrowStrips()            { return getBool ("mx_narrow", false); }
    inline bool   showInsertsInMix()        { return getBool ("mx_showInserts", true); }
    inline bool   showSendsInMix()          { return getBool ("mx_showSends", true); }

    // MIDI
    inline int    defaultVelocity()         { return getInt ("mi_velocity", 100); }
    inline double defaultQuantize()         { return getDouble ("mi_quantize", 0.25); }
    inline int    defaultNoteLengthDiv()    { return getInt ("mi_noteLen", 16); }
    inline bool   midiThru()                { return getBool ("mi_thru", true); }
    inline bool   playNotesWhenEditing()    { return getBool ("mi_auditionNotes", true); }
    inline int    midiTickResolution()      { return 960; }

    // Claude
    inline bool   claudeAutoConnect()       { return getBool ("cl_autoConnect", true); }
    inline bool   claudeMayScreenshot()     { return getBool ("cl_screenshots", true); }
    inline bool   claudeMayDeleteTracks()   { return getBool ("cl_deleteTracks", true); }
    inline bool   claudeMaySave()           { return getBool ("cl_save", true); }
    inline bool   claudeMayScanPlugins()    { return getBool ("cl_scan", true); }
    inline juce::String tunnelProvider()    { return getString ("cl_provider", "cloudflare"); }    // cloudflare | ngrok | pinggy
    inline juce::String ngrokToken()        { return getString ("cl_ngrokToken"); }
    inline juce::String ngrokDomain()       { return getString ("cl_ngrokDomain"); }
    inline bool   claudeLogVerbose()        { return getBool ("cl_verbose", false); }
}
