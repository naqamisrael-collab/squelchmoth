#pragma once
// Squelchmoth - the performance engine: Pro Tools' Playback Engine "Host Engine" settings, Dynamic Plug-in Processing,
// the disk playback cache, an overload monitor that keeps the session running when the computer runs out of
// processing power, and the System Usage window.
//
// How Squelchmoth stays up under heavy load:
//  1. CPU Usage Limit - the engine never lets the audio thread starve the Mac: above the limit it silences a block
//     instead of locking up, so the app and the UI stay responsive.
//  2. Host Processors - audio processing is spread over the chosen number of CPU cores.
//  3. Dynamic Plug-in Processing - effect plug-ins that are fed silence stop using CPU once their tails have finished.
//  4. Overload protection - when dropouts repeat, the H/W buffer is raised one step (never while recording);
//     with protection and Ignore Errors off, playback stops with an explanation, like Pro Tools.
//  5. System Usage shows the live load, dropouts, memory and the heaviest plug-ins, with Freeze / Bypass buttons.

#include "Session.h"
#include "TrackOps.h"

#if JUCE_MAC
 #include <mach/mach.h>
#endif

namespace Perf
{
    /** Resident memory of the Squelchmoth process in MB (0 if the platform does not say). */
    inline double processMemoryMB()
    {
       #if JUCE_MAC
        mach_task_basic_info info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
            return (double) info.resident_size / (1024.0 * 1024.0);
        return 0.0;
       #elif JUCE_LINUX
        auto statm = juce::File ("/proc/self/statm").loadFileAsString();
        auto parts = juce::StringArray::fromTokens (statm, " ", "");
        if (parts.size() > 1) return parts[1].getLargeIntValue() * (double) sysconf (_SC_PAGESIZE) / (1024.0 * 1024.0);
        return 0.0;
       #else
        return 0.0;
       #endif
    }

    inline juce::Array<int> bufferSizes (te::Engine& e)
    {
        juce::Array<int> out;
        if (auto* dev = e.getDeviceManager().deviceManager.getCurrentAudioDevice())
            out = dev->getAvailableBufferSizes();
        if (out.isEmpty()) out = { 32, 64, 128, 256, 512, 1024, 2048 };
        return out;
    }

    inline int currentBufferSize (te::Engine& e)
    {
        if (auto* dev = e.getDeviceManager().deviceManager.getCurrentAudioDevice()) return dev->getCurrentBufferSizeSamples();
        return 0;
    }

    inline double currentSampleRate (te::Engine& e)
    {
        if (auto* dev = e.getDeviceManager().deviceManager.getCurrentAudioDevice()) return dev->getCurrentSampleRate();
        return 0.0;
    }

    inline juce::String setBufferSize (te::Engine& e, int samples)
    {
        auto& adm = e.getDeviceManager().deviceManager;
        auto setup = adm.getAudioDeviceSetup();
        setup.bufferSize = samples;
        return adm.setAudioDeviceSetup (setup, true);
    }

    /** Applies every Host Engine preference to the running engine (and, if asked, rebuilds playback for settings
        that only take effect when the playback graph is created). */
    inline void apply (te::Engine& e, te::Edit* edit, bool rebuildPlayback)
    {
        auto& dm = e.getDeviceManager();
        dm.setCpuLimitBeforeMuting (Prefs::cpuUsageLimitPercent() / 100.0);
        dm.updateNumCPUs();
        ::dynamicPluginProcessingEnabled() = Prefs::dynamicPluginProcessing();
        te::EditPlaybackContext::setThreadPoolStrategy ((int) (Prefs::lowBufferOptimisation() ? tracktion::graph::ThreadPoolStrategy::lightweightSemHybrid
                                                                                               : tracktion::graph::ThreadPoolStrategy::semaphore));
        auto rate = currentSampleRate (e) > 0 ? currentSampleRate (e) : 48000.0;
        e.getAudioFileManager().cache.setCacheSizeSamples ((te::SampleCount) (Prefs::diskCacheSeconds() * rate));
        if (rebuildPlayback && edit != nullptr && ! edit->getTransport().isRecording())
        {
            edit->restartPlayback();
            if (! edit->getTransport().isPlaying()) edit->getTransport().ensureContextAllocated (true);
        }
    }

    //==========================================================================
    // weak references only: a window must never keep a plug-in or track alive after its session has closed
    struct HeavyPlugin { juce::String track, plugin; double cpu = 0; bool enabled = true; te::SafeSelectable<te::Plugin> ptr; te::SafeSelectable<te::Track> owner; };

    inline std::vector<HeavyPlugin> heaviestPlugins (Session& s, int maxCount = 8)
    {
        std::vector<HeavyPlugin> out;
        if (s.edit == nullptr) return out;
        auto add = [&] (te::PluginList& list, te::Track* owner, const juce::String& ownerName)
        {
            for (auto p : list.getPlugins())
            {
                if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr
                    || dynamic_cast<SqMonoSumPlugin*> (p) != nullptr || dynamic_cast<SqStereoPanPlugin*> (p) != nullptr)
                    continue;
                out.push_back ({ ownerName, Signature::displayName (*p), p->getCpuUsage(), p->isEnabled(), te::SafeSelectable<te::Plugin> (p), te::SafeSelectable<te::Track> (owner) });
            }
        };
        for (auto t : te::getAllTracks (*s.edit))
            if (! t->isMasterTrack() && t->isProcessing (true))
                add (t->pluginList, t, t->getName());
        add (s.edit->getMasterPluginList(), s.edit->getMasterTrack(), TrackOps::masterName (s));
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.cpu > b.cpu; });
        if ((int) out.size() > maxCount) out.resize ((size_t) maxCount);
        return out;
    }

    //==========================================================================
    /** Watches the audio engine 4 times a second: load, peak load, dropouts (device xruns) and overloads.
        Handles overloads according to the Playback Engine settings. */
    class Monitor : private juce::Timer
    {
    public:
        explicit Monitor (Session& s) : session (s) { startTimerHz (4); }

        float load = 0, peakLoad = 0, sessionPeak = 0;
        int dropouts = 0, overloads = 0, bufferRaises = 0;
        juce::String lastEvent;
        std::function<void (const juce::String&)> onStatus;   // short status-bar text

        void reset()  { dropouts = overloads = bufferRaises = 0; sessionPeak = 0; lastEvent = {}; recentEvents.clear(); }

    private:
        Session& session;
        int lastXruns = -1;
        juce::Array<juce::int64> recentEvents;
        juce::int64 lastRaise = 0;
        bool overloadPendingWhileRecording = false, wasPlaying = false, stoppingForOverload = false;
        juce::int64 playStart = 0, lastPosTime = 0;
        double lastPos = 0;
        int consecutiveHigh = 0;

        void timerCallback() override
        {
            auto& e = session.getEngine();
            auto& dm = e.getDeviceManager();
            load = dm.getCpuUsage();

            const auto stats = dm.getCPUStatistics();
            dm.restCPUStatistics();
            const auto rate = currentSampleRate (e);
            const auto block = currentBufferSize (e);
            const double blockSeconds = (rate > 0 && block > 0) ? block / rate : 0.0;
            peakLoad = blockSeconds > 0 ? (float) juce::jlimit (0.0, 2.0, stats.maximumSeconds / blockSeconds) : load;

            int xruns = -1;
            if (auto* dev = dm.deviceManager.getCurrentAudioDevice()) xruns = dev->getXRunCount();
            const bool newXruns = lastXruns >= 0 && xruns > lastXruns;
            if (xruns >= 0) { if (newXruns) dropouts += xruns - lastXruns; lastXruns = xruns; }

            if (session.edit == nullptr) return;
            auto& tc = session.edit->getTransport();
            if (stoppingForOverload && tc.isPlaying()) return;
            if (! tc.isPlaying())
            {
                wasPlaying = false;
                stoppingForOverload = false;
                if (overloadPendingWhileRecording)
                {
                    overloadPendingWhileRecording = false;
                    Ui::message (juce::MessageBoxIconType::WarningIcon, "CPU overload during recording",
                                 "The computer briefly ran out of processing power while you recorded (" + juce::String (overloads) + " overload(s)). "
                                 "Your take was kept. To prevent it: raise the H/W Buffer Size in Setup > Playback Engine, freeze heavy tracks "
                                 "(Window > System Usage lists the heaviest plug-ins), or make unused tracks inactive.");
                }
                return;
            }

            // ignore the first moments after Play (the playback graph is being prepared) and single stray peaks:
            // a CPU overload is two consecutive readings above the limit, or a real dropout from the interface
            const auto nowMs = juce::Time::currentTimeMillis();
            const double pos = tc.getPosition().inSeconds();
            if (! wasPlaying) { wasPlaying = true; playStart = nowMs; consecutiveHigh = 0; lastPos = pos; lastPosTime = nowMs; }
            const bool settled = nowMs - playStart > 1500;
            if (settled) sessionPeak = juce::jmax (sessionPeak, juce::jmin (1.0f, peakLoad));   // only real playback counts

            // the engine silences blocks rather than lock up the computer when the load passes the CPU Usage Limit;
            // that shows up as a load above the limit, as the playhead falling behind the clock, or as interface xruns
            const float limit = Prefs::cpuUsageLimitPercent() / 100.0f;
            const double wall = (nowMs - lastPosTime) / 1000.0, moved = pos - lastPos;
            const bool stalled = settled && wall > 0.2 && moved >= 0.0 && moved < 0.6 * wall;
            lastPos = pos; lastPosTime = nowMs;
            consecutiveHigh = (settled && (peakLoad >= limit || load >= limit)) ? consecutiveHigh + 1 : 0;
            const bool overloaded = (settled && newXruns) || stalled || consecutiveHigh >= 2;
            if (! overloaded) return;
            consecutiveHigh = 0;

            ++overloads;
            const auto now = juce::Time::currentTimeMillis();
            recentEvents.add (now);
            recentEvents.removeIf ([now] (juce::int64 t) { return now - t > 4000; });
            lastEvent = juce::Time::getCurrentTime().toString (false, true) + (newXruns ? "  interface dropout" : stalled ? "  audio could not keep up (blocks skipped)" : "  CPU peak " + juce::String (juce::roundToInt (juce::jmin (1.0f, peakLoad) * 100)) + "%");

            if (tc.isRecording()) { overloadPendingWhileRecording = true; status ("CPU overload while recording - the take continues"); return; }
            if (Prefs::ignoreErrors()) { status ("CPU overload ignored (Ignore Errors is on)"); return; }

            if (Prefs::overloadProtection())
            {
                if (recentEvents.size() >= 2 && now - lastRaise > 5000)
                {
                    auto sizes = bufferSizes (e);
                    auto cur = currentBufferSize (e);
                    int next = 0;
                    for (auto sz : sizes) if (sz > cur && sz <= 2048) { next = sz; break; }
                    if (next > 0)
                    {
                        lastRaise = now;
                        recentEvents.clear();
                        playStart = now;   // let the interface settle after reopening before judging again
                        ++bufferRaises;
                        auto err = setBufferSize (e, next);
                        status (err.isEmpty() ? "Overload protection: H/W buffer raised to " + juce::String (next) + " samples"
                                              : "Overload protection could not change the buffer: " + err);
                        session.log.add ("Squelchmoth", "Overload protection raised the H/W buffer from " + juce::String (cur) + " to " + juce::String (next) + " samples");
                    }
                    else status ("CPU overload at the largest buffer - freeze heavy tracks (Window > System Usage)");
                }
                return;
            }

            // Pro Tools behaviour: stop and explain (once - the transport takes a moment to stop)
            stoppingForOverload = true;
            tc.stop (false, false);
            Ui::message (juce::MessageBoxIconType::WarningIcon, "CPU overload",
                         "Playback stopped because the computer ran out of processing power.\n\n"
                         "Fix it in Setup > Playback Engine: raise the H/W Buffer Size, use more Host Processors, or turn on Overload Protection "
                         "or Ignore Errors. Window > System Usage shows the heaviest plug-ins - freeze or bypass those tracks.");
        }

        void status (const juce::String& s)   { if (onStatus) onStatus (s); }
    };

    //==========================================================================
    /** Setup > Playback Engine: the audio device (with channels and test tone) plus Pro Tools' Host Engine options. */
    class PlaybackEngineContent : public juce::Component, private juce::Timer
    {
    public:
        PlaybackEngineContent (Session& s, Monitor& m)
            : session (s), monitor (m),
              selector (s.getEngine().getDeviceManager().deviceManager, 0, 64, 0, 64, true, true, true, false)
        {
            addAndMakeVisible (selector);
            for (auto* l : { &hostTitle, &diskTitle })
            {
                addAndMakeVisible (l);
                l->setFont (Theme::font (13.0f, true));
                l->setColour (juce::Label::textColourId, Theme::acid());
            }
            for (auto* l : { &hostProcL, &cpuLimitL, &cacheL, &bufferL })
            {
                addAndMakeVisible (l);
                l->setFont (Theme::font (12.5f));
                l->setColour (juce::Label::textColourId, Theme::text());
            }
            addAndMakeVisible (liveL);
            liveL.setFont (Theme::font (12.0f));
            liveL.setColour (juce::Label::textColourId, Theme::dim());
            liveL.setJustificationType (juce::Justification::topLeft);

            for (auto* c : { &hostProc, &cpuLimit, &cache }) addAndMakeVisible (c);
            const int n = Prefs::numCpus();
            for (int i = 1; i <= n; ++i) hostProc.addItem (juce::String (i) + (i == 1 ? " processor" : " processors") + (i == n ? "  (all)" : ""), i);
            hostProc.setSelectedId (Prefs::hostProcessors(), juce::dontSendNotification);
            hostProc.setTooltip ("How many CPU cores Squelchmoth spreads audio processing across. More cores = more plug-ins and tracks.");
            hostProc.onChange = [this] { Prefs::set ("pe_hostProcessors", hostProc.getSelectedId() == Prefs::numCpus() ? 0 : hostProc.getSelectedId()); applyNow (false); };

            for (int pct : { 50, 60, 70, 75, 80, 85, 90, 95, 99 }) cpuLimit.addItem (juce::String (pct) + " %", pct);
            cpuLimit.setSelectedId (Prefs::cpuUsageLimitPercent(), juce::dontSendNotification);
            if (cpuLimit.getSelectedId() == 0) cpuLimit.setSelectedId (95, juce::dontSendNotification);
            cpuLimit.setTooltip ("CPU Usage Limit: the most processing time audio may take. Above it the engine skips a block instead of freezing the computer.");
            cpuLimit.onChange = [this] { Prefs::set ("pe_cpuLimit", cpuLimit.getSelectedId()); applyNow (false); };

            for (auto [secs, name] : std::initializer_list<std::pair<int, const char*>> { { 6, "Normal (6 s per file)" }, { 15, "Large (15 s per file)" }, { 30, "Extra large (30 s per file)" }, { 60, "Maximum (60 s per file)" } })
                cache.addItem (name, secs);
            cache.setSelectedId (Prefs::diskCacheSeconds(), juce::dontSendNotification);
            if (cache.getSelectedId() == 0) cache.setSelectedId (6, juce::dontSendNotification);
            cache.setTooltip ("Disk Playback Cache: how much of each audio file is read ahead into memory. Larger = smoother playback from slow or external drives, more RAM.");
            cache.onChange = [this] { Prefs::set ("pe_diskCacheSeconds", cache.getSelectedId()); applyNow (false); };

            setupToggle (ignoreErrors, "Ignore Errors During Playback/Record", "pe_ignoreErrors", Prefs::ignoreErrors(),
                         "Keep playing through CPU overloads (you may hear clicks and pops). Good for tracking ideas; turn off for final mixes.", false);
            setupToggle (dynamic, "Dynamic Plug-in Processing", "pe_dynamicPlugins", Prefs::dynamicPluginProcessing(),
                         "Plug-ins only use CPU while they have audio to process: an effect fed silence pauses once its own tail has died away, and wakes on the next sound. Bypassed plug-ins never use CPU.", false);
            setupToggle (lowBuffer, "Optimize Performance at Low Buffer Sizes", "pe_lowBufferOpt", Prefs::lowBufferOptimisation(),
                         "Keeps the audio threads awake between buffers for steadier low-latency recording. Turn off to save energy and heat on laptops.", true);
            setupToggle (protection, "Overload Protection (raise the buffer automatically)", "pe_overloadProtection", Prefs::overloadProtection(),
                         "When dropouts repeat during playback, Squelchmoth raises the H/W Buffer Size one step instead of stopping. Never changes anything while you record.", false);
            setupToggle (elastic, "Elastic Audio background processing", "pe_elasticBackground", Prefs::elasticBackground(),
                         "Time-stretching is prepared ahead on a background thread, which lowers the load on the audio thread when many Elastic Audio clips play.", true);

            addAndMakeVisible (usageBtn);
            usageBtn.onClick = [this] { if (onShowUsage) onShowUsage(); };
            addAndMakeVisible (resetBtn);
            resetBtn.onClick = [this] { monitor.reset(); timerCallback(); };
            addAndMakeVisible (defaultsBtn);
            defaultsBtn.setTooltip ("Restore the recommended Host Engine settings");
            defaultsBtn.onClick = [this]
            {
                for (auto k : { "pe_hostProcessors", "pe_cpuLimit", "pe_ignoreErrors", "pe_dynamicPlugins", "pe_lowBufferOpt", "pe_overloadProtection", "pe_elasticBackground", "pe_diskCacheSeconds" })
                    if (Prefs::file() != nullptr) Prefs::file()->removeValue (k);
                Prefs::set ("pe_defaultsRestored", juce::Time::currentTimeMillis());
                hostProc.setSelectedId (Prefs::hostProcessors(), juce::dontSendNotification);
                cpuLimit.setSelectedId (Prefs::cpuUsageLimitPercent(), juce::dontSendNotification);
                cache.setSelectedId (Prefs::diskCacheSeconds(), juce::dontSendNotification);
                ignoreErrors.setToggleState (Prefs::ignoreErrors(), juce::dontSendNotification);
                dynamic.setToggleState (Prefs::dynamicPluginProcessing(), juce::dontSendNotification);
                lowBuffer.setToggleState (Prefs::lowBufferOptimisation(), juce::dontSendNotification);
                protection.setToggleState (Prefs::overloadProtection(), juce::dontSendNotification);
                elastic.setToggleState (Prefs::elasticBackground(), juce::dontSendNotification);
                applyNow (true);
            };

            setSize (1000, 660);
            startTimerHz (4);
            timerCallback();
        }

        std::function<void()> onShowUsage;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel());
            g.setColour (Theme::line());
            g.drawVerticalLine (selectorWidth + 8, 10.0f, (float) getHeight() - 10.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            selector.setBounds (r.removeFromLeft (selectorWidth).reduced (4));
            r.removeFromLeft (16);
            r = r.reduced (8, 12);
            auto line = [&] (juce::Component& c, int h = 24) { c.setBounds (r.removeFromTop (h)); r.removeFromTop (4); };
            line (hostTitle, 22);
            line (bufferL, 20);
            auto pair = [&] (juce::Label& l, juce::ComboBox& c) { auto row = r.removeFromTop (26); l.setBounds (row.removeFromLeft (150)); c.setBounds (row); r.removeFromTop (6); };
            pair (hostProcL, hostProc);
            pair (cpuLimitL, cpuLimit);
            for (auto* t : { &ignoreErrors, &dynamic, &lowBuffer, &protection, &elastic }) line (*t, 26);
            r.removeFromTop (6);
            line (diskTitle, 22);
            pair (cacheL, cache);
            r.removeFromTop (8);
            auto btns = r.removeFromBottom (30);
            usageBtn.setBounds (btns.removeFromLeft (130)); btns.removeFromLeft (8);
            resetBtn.setBounds (btns.removeFromLeft (130)); btns.removeFromLeft (8);
            defaultsBtn.setBounds (btns.removeFromLeft (110));
            r.removeFromBottom (8);
            liveL.setBounds (r);
        }

    private:
        static constexpr int selectorWidth = 480;
        Session& session;
        Monitor& monitor;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label hostTitle { {}, "HOST ENGINE" }, diskTitle { {}, "DISK PLAYBACK" },
                    hostProcL { {}, "Host Processors" }, cpuLimitL { {}, "CPU Usage Limit" }, cacheL { {}, "Cache Size" }, bufferL, liveL;
        juce::ComboBox hostProc, cpuLimit, cache;
        juce::ToggleButton ignoreErrors, dynamic, lowBuffer, protection, elastic;
        juce::TextButton usageBtn { "System Usage..." }, resetBtn { "Reset counters" }, defaultsBtn { "Defaults" };

        void setupToggle (juce::ToggleButton& b, const juce::String& text, const char* key, bool state, const juce::String& tip, bool rebuild)
        {
            addAndMakeVisible (b);
            b.setButtonText (text);
            b.setToggleState (state, juce::dontSendNotification);
            b.setTooltip (tip);
            b.onClick = [this, &b, key, rebuild] { Prefs::set (key, b.getToggleState()); applyNow (rebuild); };
        }

        void applyNow (bool rebuild)
        {
            apply (session.getEngine(), session.edit.get(), rebuild);
            session.log.add ("You", "Playback Engine settings changed");
        }

        void timerCallback() override
        {
            auto& e = session.getEngine();
            auto rate = currentSampleRate (e);
            auto buf = currentBufferSize (e);
            bufferL.setText ("H/W Buffer Size: " + juce::String (buf) + " samples" + (rate > 0 ? "  (" + juce::String (1000.0 * buf / rate, 1) + " ms)" : juce::String())
                             + "  -  change it in the Audio buffer size menu on the left", juce::dontSendNotification);
            juce::String t;
            t << "Audio engine load now: " << juce::roundToInt (monitor.load * 100) << " %    peak: " << juce::roundToInt (juce::jmin (1.0f, monitor.peakLoad) * 100) << " %"
              << "    session peak: " << juce::roundToInt (monitor.sessionPeak * 100) << " %\n"
              << "Overloads: " << monitor.overloads << "    dropouts reported by the interface: " << monitor.dropouts
              << "    automatic buffer raises: " << monitor.bufferRaises << "\n"
              << "Dynamic Plug-in Processing has skipped " << (juce::int64) ::dynamicPluginBlocksSkipped().load() << " silent plug-in blocks\n"
              << "Memory: Squelchmoth " << juce::roundToInt (processMemoryMB()) << " MB    disk cache " << juce::roundToInt (e.getAudioFileManager().cache.getBytesInUse() / (1024.0 * 1024.0)) << " MB"
              << "    computer " << juce::SystemStats::getMemorySizeInMegabytes() / 1024 << " GB, " << Prefs::numCpus() << " cores\n";
            if (monitor.lastEvent.isNotEmpty()) t << "Last overload: " << monitor.lastEvent << "\n";
            t << "\nTips: 128 samples or lower for recording and playing instruments; 512-1024 for mixing big sessions. "
                 "Freeze tracks whose plug-ins you have finished with, and make unused tracks inactive.";
            liveL.setText (t, juce::dontSendNotification);
        }
    };

    //==========================================================================
    /** Window > System Usage: live load meters, dropouts, memory and the heaviest plug-ins with one-click fixes. */
    class SystemUsageContent : public juce::Component, private juce::Timer
    {
    public:
        SystemUsageContent (Session& s, Monitor& m) : session (s), monitor (m)
        {
            for (int i = 0; i < maxRows; ++i)
            {
                auto& r = rows[(size_t) i];
                addChildComponent (r.freeze);
                addChildComponent (r.bypass);
                r.freeze.onClick = [this, i] { act (i, true); };
                r.bypass.onClick = [this, i] { act (i, false); };
            }
            addAndMakeVisible (engineBtn);
            engineBtn.onClick = [this] { if (onShowEngine) onShowEngine(); };
            addAndMakeVisible (resetBtn);
            resetBtn.onClick = [this] { monitor.reset(); repaint(); };
            setSize (560, 470);
            startTimerHz (4);
            timerCallback();
        }

        std::function<void()> onShowEngine;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel());
            auto& e = session.getEngine();
            auto r = getLocalBounds().reduced (16, 12);

            auto meter = [&] (const juce::String& label, float v, const juce::String& text)
            {
                auto row = r.removeFromTop (26);
                g.setColour (Theme::text());
                g.setFont (Theme::font (12.5f, true));
                g.drawText (label, row.removeFromLeft (120), juce::Justification::centredLeft);
                auto bar = row.removeFromLeft (row.getWidth() - 120).reduced (0, 5).toFloat();
                g.setColour (Theme::bg());
                g.fillRoundedRectangle (bar, 3.0f);
                auto c = v < 0.6f ? Theme::mint() : v < 0.85f ? Theme::amber() : Theme::red();
                g.setColour (c);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);
                g.setColour (Theme::dim());
                g.setFont (Theme::font (12.0f));
                g.drawText (text, row.withTrimmedLeft (10), juce::Justification::centredLeft);
                r.removeFromTop (4);
            };

            const float limit = Prefs::cpuUsageLimitPercent() / 100.0f;
            meter ("Audio CPU", monitor.load, juce::String (juce::roundToInt (monitor.load * 100)) + " %");
            meter ("Peak", juce::jmin (1.0f, monitor.peakLoad), juce::String (juce::roundToInt (juce::jmin (1.0f, monitor.peakLoad) * 100)) + " % / limit " + juce::String (juce::roundToInt (limit * 100)) + " %");
            const double memGB = juce::SystemStats::getMemorySizeInMegabytes() / 1024.0;
            const double mine = processMemoryMB();
            meter ("Memory", memGB > 0 ? (float) (mine / 1024.0 / memGB) : 0.0f, juce::String (juce::roundToInt (mine)) + " MB of " + juce::String (juce::roundToInt (memGB)) + " GB");
            const double cacheMB = e.getAudioFileManager().cache.getBytesInUse() / (1024.0 * 1024.0);
            meter ("Disk cache", (float) juce::jmin (1.0, cacheMB / 2048.0), juce::String (juce::roundToInt (cacheMB)) + " MB");

            g.setColour (Theme::text());
            g.setFont (Theme::font (12.5f));
            auto info = r.removeFromTop (40);
            g.drawFittedText ("Overloads " + juce::String (monitor.overloads) + "   -   interface dropouts " + juce::String (monitor.dropouts)
                              + "   -   buffer " + juce::String (currentBufferSize (e)) + " samples   -   " + juce::String (Prefs::hostProcessors()) + " of "
                              + juce::String (Prefs::numCpus()) + " cores   -   Dynamic Plug-in Processing " + (Prefs::dynamicPluginProcessing() ? "on" : "off"),
                              info, juce::Justification::centredLeft, 2);

            g.setColour (Theme::acid());
            g.setFont (Theme::font (13.0f, true));
            g.drawText ("HEAVIEST PLUG-INS (share of each audio buffer)", r.removeFromTop (22), juce::Justification::centredLeft);
            listTop = r.getY();
            g.setFont (Theme::font (12.5f));
            for (int i = 0; i < (int) heavy.size(); ++i)
            {
                auto row = juce::Rectangle<int> (r.getX(), listTop + i * rowH, r.getWidth() - 170, rowH);
                g.setColour (i % 2 ? Theme::panel2() : Theme::panel());
                g.fillRect (row);
                g.setColour (Theme::text());
                g.drawText (heavy[(size_t) i].plugin + "   on " + heavy[(size_t) i].track, row.withTrimmedLeft (6).withTrimmedRight (60), juce::Justification::centredLeft, true);
                g.setColour (heavy[(size_t) i].cpu > 0.25 ? Theme::red() : Theme::dim());
                g.drawText (juce::String (heavy[(size_t) i].cpu * 100.0, 1) + " %", row.removeFromRight (56), juce::Justification::centredRight);
            }
            if (heavy.empty())
            {
                g.setColour (Theme::dim());
                g.drawText ("Press play: plug-in loads appear while the session runs.", r.removeFromTop (rowH), juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (16, 12).removeFromBottom (30);
            engineBtn.setBounds (b.removeFromLeft (150)); b.removeFromLeft (8);
            resetBtn.setBounds (b.removeFromLeft (130));
            layoutRows();
        }

    private:
        static constexpr int maxRows = 8, rowH = 24;
        Session& session;
        Monitor& monitor;
        std::vector<HeavyPlugin> heavy;
        int listTop = 220;
        struct Row { juce::TextButton freeze { "Freeze track" }, bypass { "Bypass" }; };
        std::array<Row, maxRows> rows;
        juce::TextButton engineBtn { "Playback Engine..." }, resetBtn { "Reset counters" };

        void layoutRows()
        {
            auto x = getWidth() - 16 - 166;
            for (int i = 0; i < maxRows; ++i)
            {
                auto& r = rows[(size_t) i];
                const bool show = i < (int) heavy.size();
                r.freeze.setVisible (show && dynamic_cast<te::AudioTrack*> (heavy[(size_t) i].owner.get()) != nullptr);
                r.bypass.setVisible (show);
                r.freeze.setBounds (x, listTop + i * rowH + 1, 96, rowH - 2);
                r.bypass.setBounds (x + 100, listTop + i * rowH + 1, 66, rowH - 2);
                if (show) r.bypass.setButtonText (heavy[(size_t) i].enabled ? "Bypass" : "Enable");
            }
        }

        void act (int i, bool freeze)
        {
            if (i >= (int) heavy.size()) return;
            auto& h = heavy[(size_t) i];
            if (freeze)
            {
                if (auto at = dynamic_cast<te::AudioTrack*> (h.owner.get()))
                {
                    auto err = TrackOps::freeze (*at, ! at->isFrozen (te::Track::individualFreeze));
                    session.log.add ("You", (err.isEmpty() ? "Froze " : "Could not freeze ") + at->getName() + (err.isEmpty() ? juce::String() : ": " + err));
                }
            }
            else if (auto* p = h.ptr.get())
                p->setEnabled (! p->isEnabled());
            timerCallback();
        }

        void timerCallback() override
        {
            heavy = heaviestPlugins (session, maxRows);
            heavy.erase (std::remove_if (heavy.begin(), heavy.end(), [] (auto& h) { return h.cpu < 0.0005 && h.enabled; }), heavy.end());
            layoutRows();
            repaint();
        }
    };
}
