// SQUELCHMOTH - a Tracktion Engine powered DAW with a built-in Claude control server.

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include "httplib.h"   // winsock2 must come before windows.h
#endif
#include <JuceHeader.h>
// Shortcut names in UI text: Cmd / Opt on the Mac, Ctrl / Alt on Windows and Linux.
#if JUCE_MAC
 #define SQ_CMD "Cmd"
 #define SQ_OPT "Opt"
 #define SQ_FILE_BROWSER_SHOW "Show file in Finder"
 #define SQ_FILE_BROWSER_SHOW_SESSION "Show session file in Finder"
#else
 #define SQ_FILE_BROWSER_SHOW "Show file in Explorer"
 #define SQ_FILE_BROWSER_SHOW_SESSION "Show session file in Explorer"
 #define SQ_CMD "Ctrl"
 #define SQ_OPT "Alt"
#endif
#include "StereoPan.h"
#include "Analyzer.h"
#include "MainComponent.h"
#include "Licence.h"
#include "SelfTest.h"

class SquelchmothApp : public juce::JUCEApplication,
                       private juce::Timer
{
public:
    const juce::String getApplicationName() override       { return "Squelchmoth"; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    // Plugin-scan child processes and the self-test are extra copies of this binary, so they must be allowed.
    bool moreThanOneInstanceAllowed() override
    {
        auto cl = getCommandLineParameters();
        return cl.contains ("--selftest") || cl.contains ("--preset-audit") || cl.contains ("--render-faces") || cl.contains ("--make-") || cl.contains ("--check-key") || cl.contains ("--dump-catalog") || cl.contains ("PluginScan");
    }

    void initialise (const juce::String& commandLine) override
    {
       #if JUCE_MAC
        // Plug-in scanning and the command-line modes are extra copies of this binary: on macOS each would
        // otherwise add its own Squelchmoth icon to the Dock next to the real app.
        if (commandLine.contains ("PluginScan") || commandLine.contains ("--selftest") || commandLine.contains ("--preset-audit") || commandLine.contains ("--render-faces")
            || commandLine.contains ("--make-") || commandLine.contains ("--check-key") || commandLine.contains ("--dump-catalog"))
            juce::Process::setDockIconVisible (false);
       #endif
        // Out-of-process plugin scanning re-launches this binary with special arguments.
        if (te::PluginManager::startChildProcessPluginScan (commandLine))
            return;

        if (commandLine.contains ("--make-keypair"))
        {
            juce::RSAKey pub, priv;
            juce::RSAKey::createKeyPair (pub, priv, 2048);
            std::cout << "PUBLIC  " << pub.toString() << "\nPRIVATE " << priv.toString() << "\n";
            quit();
            return;
        }

        if (commandLine.contains ("--make-licence"))
        {
            // Squelchmoth --make-licence "<private key>" "<name>" "<email>"
            auto args = juce::ArgumentList ("Squelchmoth", commandLine);
            auto i = -1;
            for (int k = 0; k < args.size(); ++k) if (args[k] == "--make-licence") i = k;
            if (i < 0 || i + 3 >= args.size() + 0) { std::cout << "usage: --make-licence <privateKey> <name> <email>\n"; setApplicationReturnValue (1); quit(); return; }
            std::cout << Licence::makeKey (args[i + 1].text, args[i + 2].text, args[i + 3].text) << "\n";
            quit();
            return;
        }

        if (commandLine.contains ("--check-key"))
        {
            // Squelchmoth --check-key <SQM2-...> : verify a store key against the shipped store public key (no activation, nothing stored)
            auto args = juce::ArgumentList ("Squelchmoth", commandLine);
            int ok = 0, bad = 0;
            for (int k = 0; k < args.size(); ++k)
                if (args[k].text.startsWith ("SQM2-"))
                {
                    juce::String product, serial;
                    const bool v = Licence::verifyStoreKey (args[k].text, product, serial) && ! Licence::revokedSerials().contains (serial);
                    std::cout << (v ? "VALID   " : (Licence::revokedSerials().contains (serial) ? "REVOKED " : "INVALID ")) << product << " serial " << serial << "\n";
                    (v ? ok : bad)++;
                }
            std::cout << ok << " valid, " << bad << " invalid\n";
            setApplicationReturnValue (bad == 0 && ok > 0 ? 0 : 1);
            quit();
            return;
        }

        if (commandLine.contains ("--dump-catalog"))
        {
            // Squelchmoth --dump-catalog out.json   (every Signature plug-in with its type, category and preset names; tooling)
            auto args = juce::ArgumentList ("Squelchmoth", commandLine);
            juce::File out;
            for (int i = 0; i < args.size(); ++i) if (args[i].text == "--dump-catalog" && i + 1 < args.size()) out = juce::File (args[i + 1].text.unquoted());
            juce::Array<juce::var> arr;
            for (auto& e : Signature::entries())
            {
                auto* o = new juce::DynamicObject(); o->setProperty ("name", e.name); o->setProperty ("type", e.type); o->setProperty ("category", e.category);
                juce::Array<juce::var> ps; for (auto& pr : e.presets) ps.add (juce::String (pr.name)); o->setProperty ("presets", ps);
                arr.add (juce::var (o));
            }
            const auto txt = juce::JSON::toString (juce::var (arr));
            if (out != juce::File()) out.replaceWithText (txt); else std::cout << txt << "\n";
            setApplicationReturnValue (0); quit(); return;
        }

        if (commandLine.contains ("--preset-audit"))
        {
            // Squelchmoth --preset-audit [report.md] [--only "Plug-in Name,Other Name"]
            auto args = juce::ArgumentList ("Squelchmoth", commandLine);
            SelfTestMode::presetAuditOnly = true;
            for (int i = 0; i < args.size(); ++i)
            {
                if (args[i].text == "--preset-audit" && i + 1 < args.size() && ! args[i + 1].text.startsWith ("--")) SelfTestMode::reportPath = args[i + 1].text.unquoted();
                if (args[i].text == "--only" && i + 1 < args.size()) SelfTestMode::only = juce::StringArray::fromTokens (args[i + 1].text.unquoted(), ",", "\"");
            }
            for (auto& o : SelfTestMode::only) o = o.trim();
            setApplicationReturnValue (runSelfTest());
            quit();
            return;
        }

        if (commandLine.contains ("--render-faces"))
        {
            // Squelchmoth --render-faces [dir] [--only "Plug-in Name,Other Name"]   (design QA: every face to a PNG)
            auto args = juce::ArgumentList ("Squelchmoth", commandLine);
            SelfTestMode::renderFaces = true;
            for (int i = 0; i < args.size(); ++i)
            {
                if (args[i].text == "--render-faces" && i + 1 < args.size() && ! args[i + 1].text.startsWith ("--")) SelfTestMode::facesDir = args[i + 1].text.unquoted();
                if (args[i].text == "--only" && i + 1 < args.size()) SelfTestMode::only = juce::StringArray::fromTokens (args[i + 1].text.unquoted(), ",", "\"");
            }
            for (auto& o : SelfTestMode::only) o = o.trim();
            setApplicationReturnValue (runSelfTest());
            quit();
            return;
        }

        if (commandLine.contains ("--selftest"))
        {
            setApplicationReturnValue (runSelfTest());
            quit();
            return;
        }

        juce::PropertiesFile::Options opts;
        opts.applicationName = "Squelchmoth";
        opts.filenameSuffix = ".settings";
        opts.folderName = "Squelchmoth";
        opts.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (opts);
        Prefs::file() = props.get();
        Theme::load (*props);
        licence = std::make_unique<Licence> (*props);
        // crash protection: learn what happened last time before anything (or any plug-in) is loaded
        CrashGuard::begin (props->getFile().getParentDirectory(), juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown());

        auto ui = std::make_unique<SquelchmothUIBehaviour>();
        auto* uiRaw = ui.get();
        engine = std::make_unique<te::Engine> ("Squelchmoth", std::move (ui), std::make_unique<SquelchmothEngineBehaviour>());
        engine->getPluginManager().initialiseAirWindows();
        engine->getPluginManager().createBuiltInType<te::ImpulseResponsePlugin>();   // Cavern IR (convolution)
        engine->getPluginManager().createBuiltInType<te::ToneGeneratorPlugin>();     // signal generator for the mastering page
        engine->getPluginManager().createBuiltInType<SqStereoPanPlugin>();           // hidden dual panner
        engine->getPluginManager().createBuiltInType<SqMonoSumPlugin>();             // hidden: makes mono tracks mono
        engine->getPluginManager().createBuiltInType<SqAnalyzerPlugin>();            // mastering meter tap
        engine->getPluginManager().createBuiltInType<SqStripTapPlugin>();            // console strip meters (RMS / LUFS)
        engine->getPluginManager().createBuiltInType<Sq808Plugin>();                 // Moth 808
        engine->getPluginManager().createBuiltInType<SqBlossomPlugin>();             // Blossom physical-model strings
        engine->getPluginManager().createBuiltInType<SqSynth::SqMothsynth2Plugin>(); // 1.8 instrument shelf
        engine->getPluginManager().createBuiltInType<SqSynth::SqVenomPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqSimMothPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqMiniMothPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqThumpPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqDial10Plugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqHexwingPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqToxinPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqLanternPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqLanternExoPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqLarvaBassPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqThornBassPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqCarapacePlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqLiveDrumSynthPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqDrumPadModelPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqThumpLivePlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqChrysalis2Plugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqChrysalisSFPlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqMandiblePlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqMandibleLitePlugin>();
        engine->getPluginManager().createBuiltInType<SqSynth::SqPollenPlugin>();
        engine->getPluginManager().createBuiltInType<SqTimeWeaverPlugin>();
        engine->getPluginManager().createBuiltInType<SqLarvaFilterPlugin>();
        engine->getPluginManager().createBuiltInType<SqApexTriadPlugin>();
        engine->getPluginManager().createBuiltInType<SqRackChannelPlugin>();
        engine->getPluginManager().createBuiltInType<SqHivePadsPlugin>();           // Hive Pads pad sampler         // Channel Settings (MIDI, slot 0 of rack tracks)
        engine->getPluginManager().createBuiltInType<SqTunePlugin>();                // Moth Tune vocal tuner
        engine->getPluginManager().createBuiltInType<SqTuneFxPlugin>();              // Moth Tune FX
        engine->getPluginManager().createBuiltInType<SqApexDynamicsPlugin>();        // Apex Series
        engine->getPluginManager().createBuiltInType<SqApexLimiterPlugin>();
        engine->getPluginManager().createBuiltInType<SqApexMultibandPlugin>();
        engine->getPluginManager().createBuiltInType<SqApexSubPlugin>();
        engine->getPluginManager().createBuiltInType<SqAuroraPlugin>();
        engine->getPluginManager().createBuiltInType<SqOptoPlugin>();
        engine->getPluginManager().createBuiltInType<SqPulseEqPlugin>();
        engine->getPluginManager().createBuiltInType<SqOilCanPlugin>();
        engine->getPluginManager().createBuiltInType<SqLadderPlugin>();
        engine->getPluginManager().createBuiltInType<SqCinderPlugin>();             // 1.11 Color & Space
        engine->getPluginManager().createBuiltInType<SqWingbeatPlugin>();
        engine->getPluginManager().createBuiltInType<SqWingbeatJrPlugin>();
        engine->getPluginManager().createBuiltInType<SqFoilPlatePlugin>();
        engine->getPluginManager().createBuiltInType<SqFoilPlateLitePlugin>();
        engine->getPluginManager().createBuiltInType<SqRadiantPlugin>();
        engine->getPluginManager().createBuiltInType<SqRadiantPrePlugin>();
        engine->getPluginManager().createBuiltInType<SqMawPlugin>();
        engine->getPluginManager().createBuiltInType<SqMawLitePlugin>();
        engine->getPluginManager().createBuiltInType<SqVellumPlugin>();
        engine->getPluginManager().createBuiltInType<SqTremorPlugin>();
        engine->getPluginManager().createBuiltInType<SqOrbitPanPlugin>();
        engine->getPluginManager().createBuiltInType<SqMirrorShiftPlugin>();
        engine->getPluginManager().createBuiltInType<SqMirrorShiftLitePlugin>();
        engine->getPluginManager().createBuiltInType<SqShutterPlugin>();
        engine->getPluginManager().createBuiltInType<SqShutterLitePlugin>();
        engine->getPluginManager().createBuiltInType<SqShardPlugin>();
        engine->getPluginManager().createBuiltInType<SqMurmurationPlugin>();
        engine->getPluginManager().createBuiltInType<SqMasqueradePlugin>();
        engine->getPluginManager().createBuiltInType<SqLarvaFilter2Plugin>();
        engine->getPluginManager().createBuiltInType<SqTwelvePhase2Plugin>();
        engine->getPluginManager().createBuiltInType<SqHiveRackPlugin>();
        engine->getPluginManager().createBuiltInType<SqMothKeyPlugin>();
        engine->getPluginManager().createBuiltInType<SqSirenCompPlugin>();
        engine->getPluginManager().createBuiltInType<SqSirenEqPlugin>();
        engine->getPluginManager().createBuiltInType<SqHushDeEss2Plugin>();
        engine->getPluginManager().createBuiltInType<SqRingPlugin>();
        engine->getPluginManager().createBuiltInType<SqPhase12Plugin>();
        engine->getPluginManager().createBuiltInType<SqBucketPlugin>();
        te::setDefaultPanLaw ((te::PanLaw) Prefs::getInt ("mx_panLaw", (int) te::PanLaw3dBCenter));

        session = std::make_unique<Session> (*engine);
        Skins::presetsRoot() = session->sessionsDir.getChildFile ("Plugin Presets");
        session->canSaveAndBounce = [this] { return licence->canSaveAndBounce(); };
        session->pickColour = [] (int index, const juce::String& kind)
        {
            // audio tracks walk the palette; instruments / busses take fixed accents so track types read at a glance
            if (kind == "instrument") return Theme::violet();
            if (kind == "bus") return Theme::mint();
            return Theme::nextTrackColour (index);
        };
        uiRaw->getEdit = [this] { return session->edit.get(); };
        uiRaw->onMessage = [this] (const juce::String& m) { session->log.add ("Engine", m); };
        session->onEditOpened = [this] (const juce::File& f)
        {
            props->setValue ("lastSession", f.getFullPathName());
            props->saveIfNeeded();
        };

        scanner = std::make_unique<PluginScanner> (*engine);
        if (scanner->getRestoredFromBackup() > 0) session->log.add ("Squelchmoth", "Restored " + juce::String (scanner->getRestoredFromBackup()) + " plug-in(s) from the plug-in list backup");
        tools = std::make_unique<McpTools> (*session, *scanner);
        server = std::make_unique<McpServer> (*tools, *props);
        tunnel = std::make_unique<Tunnel>();

        // Open the last session, or a file passed on the command line, or make a new one.
        juce::File toOpen (commandLine.unquoted().trim());
        if (! toOpen.existsAsFile())
            toOpen = juce::File (props->getValue ("lastSession"));
        if (! (toOpen.existsAsFile() && session->openSession (toOpen)))
            session->newSession ("My First Session");

        if (server->start())
            session->log.add ("Squelchmoth", "Claude control server listening on " + server->getLocalUrl().upToLastOccurrenceOf ("/mcp/", false, false));
        else
            session->log.add ("Squelchmoth", "Could not start the Claude control server: " + server->getLastError(), false);

        if (Prefs::claudeAutoConnect())
            tunnel->startTunnel (server->getPort());

        mainWindow = std::make_unique<MainWindow> (*session, *tools, *server, *tunnel, *scanner, *props, *licence);
        if (auto why = CrashGuard::explanation(); why.isNotEmpty())
        {
            session->log.add ("Squelchmoth", why.upToFirstOccurrenceOf ("\n", false, false), false);
            juce::Timer::callAfterDelay (1200, [why] { Ui::message (juce::MessageBoxIconType::WarningIcon,
                                                                    CrashGuard::state().crashedLastTime ? "Squelchmoth recovered" : "Safe Mode", why); });
        }
        session->log.add ("Squelchmoth", licence->statusText());
        if (licence->isDemo())
            session->log.add ("Squelchmoth", "Demo edition: the basics, free forever. Help > Licences and Upgrades unlocks the full program and the add-on packs.");
        // Auto-Save + Auto Backup (never while playing or recording) and, after a crash, an offer of the newest backup
        autoSaver = std::make_unique<AutoSaver> (*session);
        if (CrashGuard::state().crashedLastTime)
            juce::Timer::callAfterDelay (2500, [this] { if (session != nullptr) AutoSaver::offerRecovery (*session); });

        // Plugins are found automatically: every launch scans for anything new (only new files are opened).
        if (Prefs::getBool ("pl_scanAtLaunch", true) || ! props->getBoolValue ("didFirstScan", false))
        {
            bool first = ! props->getBoolValue ("didFirstScan", false);
            props->setValue ("didFirstScan", true);
            props->saveIfNeeded();
            juce::Timer::callAfterDelay (1500, [this, first]
            {
                scanner->startScan (false);
                session->log.add ("Squelchmoth", first ? "First launch: finding your VST3 / Audio Unit plugins in the background (Setup > Plugins shows progress)."
                                                       : "Checking for new plugins in the background.");
            });
        }
    }

    void shutdown() override
    {
        if (session != nullptr && props != nullptr)
        {
            props->setValue ("lastSession", session->getFile().getFullPathName());
            props->saveIfNeeded();
        }
        mainWindow.reset();
        autoSaver.reset();
        tunnel.reset();
        server.reset();
        tools.reset();
        scanner.reset();
        session.reset();
        engine.reset();
        CrashGuard::end();
    }

    void systemRequestedQuit() override     { quit(); }

    void timerCallback() override {}

    void anotherInstanceStarted (const juce::String& cl) override
    {
        juce::File f (cl.unquoted().trim());
        if (f.existsAsFile() && session) session->openSession (f);
    }

    class MainWindow : public juce::DocumentWindow, private juce::Timer
    {
    public:
        MainWindow (Session& s, McpTools& t, McpServer& srv, Tunnel& tun, PluginScanner& sc, juce::PropertiesFile& p, Licence& lic)
            : DocumentWindow ("Squelchmoth - " + s.getFile().getFileNameWithoutExtension(), Theme::bg(), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            sqmMainWindow() = this;   // secondary windows attach to this one (one taskbar icon)
            setContentOwned (new MainComponent (s, t, srv, tun, sc, p, lic), true);
            setResizable (true, false);
            setResizeLimits (1000, 640, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            getContentComponent()->grabKeyboardFocus();
           #if JUCE_WINDOWS
            startTimerHz (4);   // any new editor / dialog window joins the main window's single taskbar icon
           #endif
        }

        ~MainWindow() override              { stopTimer(); sqmDetachAll(); if (sqmMainWindow() == this) sqmMainWindow() = nullptr; }
        void timerCallback() override       { sqmAttachAll(); }
        void closeButtonPressed() override  { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<juce::PropertiesFile> props;
    std::unique_ptr<Licence> licence;
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<Session> session;
    std::unique_ptr<PluginScanner> scanner;
    std::unique_ptr<McpTools> tools;
    std::unique_ptr<McpServer> server;
    std::unique_ptr<Tunnel> tunnel;
    std::unique_ptr<AutoSaver> autoSaver;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SquelchmothApp)
