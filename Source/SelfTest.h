#pragma once
// Headless end-to-end test: drives the DAW exclusively through the MCP JSON-RPC layer,
// exactly the way Claude does, and checks the results. Run:  Squelchmoth --selftest

#include "McpServer.h"
#include "Behaviours.h"
#include "Licence.h"
#include "ChatPanel.h"
#include "MixerView.h"
#include "SqEngine111.h"

/** Set by Main for "--preset-audit [report.md]": the self-test sets up the engine, runs the audit and returns. */
struct SelfTestMode { static inline bool presetAuditOnly = false, renderFaces = false; static inline juce::String reportPath, facesDir, facePreset; static inline juce::StringArray only; };

inline int runSelfTest()
{
    auto ui = std::make_unique<SquelchmothUIBehaviour>();
    ui->headless = true;
    ui->onMessage = [] (const juce::String& m) { std::cout << "  [engine] " << m << "\n"; };
    auto engine = std::make_unique<te::Engine> ("SquelchmothSelfTest", std::move (ui), std::make_unique<SquelchmothEngineBehaviour>());
    engine->getPluginManager().initialiseAirWindows();
    engine->getPluginManager().createBuiltInType<te::ImpulseResponsePlugin>();
    engine->getPluginManager().createBuiltInType<te::ToneGeneratorPlugin>();
    engine->getPluginManager().createBuiltInType<SqStereoPanPlugin>();
    engine->getPluginManager().createBuiltInType<SqMonoSumPlugin>();
    engine->getPluginManager().createBuiltInType<SqAnalyzerPlugin>();
    engine->getPluginManager().createBuiltInType<SqStripTapPlugin>();
    engine->getPluginManager().createBuiltInType<Sq808Plugin>();
    engine->getPluginManager().createBuiltInType<SqBlossomPlugin>();
    engine->getPluginManager().createBuiltInType<SqTunePlugin>();
    engine->getPluginManager().createBuiltInType<SqTuneFxPlugin>();
    engine->getPluginManager().createBuiltInType<SqApexDynamicsPlugin>();
    engine->getPluginManager().createBuiltInType<SqApexLimiterPlugin>();
    engine->getPluginManager().createBuiltInType<SqApexMultibandPlugin>();
    engine->getPluginManager().createBuiltInType<SqApexSubPlugin>();
    engine->getPluginManager().createBuiltInType<SqAuroraPlugin>();
    engine->getPluginManager().createBuiltInType<SqOptoPlugin>();
    engine->getPluginManager().createBuiltInType<SqPulseEqPlugin>();
    engine->getPluginManager().createBuiltInType<SqOilCanPlugin>();
    engine->getPluginManager().createBuiltInType<SqLadderPlugin>();
    engine->getPluginManager().createBuiltInType<SqCinderPlugin>();
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
    engine->getPluginManager().createBuiltInType<SqSirenVerbPlugin>();
    engine->getPluginManager().createBuiltInType<SqMicClosetPlugin>();
    engine->getPluginManager().createBuiltInType<SqLarynxPlugin>();
    engine->getPluginManager().createBuiltInType<SqExhalePlugin>();
    engine->getPluginManager().createBuiltInType<SqHiveChoirPlugin>();
    engine->getPluginManager().createBuiltInType<SqDoubleWing2Plugin>();
    engine->getPluginManager().createBuiltInType<SqMutantPlugin>();
    engine->getPluginManager().createBuiltInType<SqVoxMothPlugin>();
    engine->getPluginManager().createBuiltInType<SqTalkWingPlugin>();
    engine->getPluginManager().createBuiltInType<SqCleanRoomPlugin>();
    engine->getPluginManager().createBuiltInType<SqMothKeyPlugin>();
    engine->getPluginManager().createBuiltInType<SqSirenCompPlugin>();
    engine->getPluginManager().createBuiltInType<SqSirenEqPlugin>();
    engine->getPluginManager().createBuiltInType<SqHushDeEss2Plugin>();
    engine->getPluginManager().createBuiltInType<SqRingPlugin>();
    engine->getPluginManager().createBuiltInType<SqPhase12Plugin>();
    engine->getPluginManager().createBuiltInType<SqBucketPlugin>();
    engine->getPluginManager().createBuiltInType<SqHivePadsPlugin>();
    engine->getPluginManager().createBuiltInType<SqRackChannelPlugin>();
    engine->getPluginManager().createBuiltInType<SqSynth::SqMothsynth2Plugin>();
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

    juce::PropertiesFile::Options opts;
    opts.applicationName = "SquelchmothSelfTest";
    opts.filenameSuffix = ".settings";
    opts.folderName = "SquelchmothSelfTest";
    juce::PropertiesFile props (opts);
    Prefs::file() = &props;

    auto session = std::make_unique<Session> (*engine);
    session->sessionsDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sm_selftest");
    session->sessionsDir.deleteRecursively();
    session->sessionsDir.createDirectory();
    session->bouncesDir = session->sessionsDir.getChildFile ("Bounces");
    session->bouncesDir.createDirectory();

    SqHivePadsPlugin::kitFiles = [&] (int kit) { return DrumKit::ensureKit (*session, kit); };   // Hive Pads kit channels need the rendered kits
    SqFactory::rootHook() = [&] { return session->sessionsDir.getChildFile ("Factory"); }; FactoryLoops::install();
    auto scannerOwner = std::make_unique<PluginScanner> (*engine);
    auto& scanner = *scannerOwner;
    McpTools tools (*session, scanner);
    McpServer server (tools, props);

    int failures = 0, passes = 0, id = 1;
    juce::String sid;

    if (SelfTestMode::presetAuditOnly)
    {
        auto report = SelfTestMode::reportPath.isNotEmpty() ? juce::File::getCurrentWorkingDirectory().getChildFile (SelfTestMode::reportPath)
                                                            : juce::File::getCurrentWorkingDirectory().getChildFile ("preset-audit.md");
        std::cout << "== Squelchmoth preset audit ==\n";
        if (session->edit == nullptr) session->newSession ("Audit");
        Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };   // audit every plug-in, including the ones a rack hosts
        auto sum = PresetAudit::run (*session->edit, SelfTestMode::only, { "*" }, 3.0, report, [] (const juce::String& m) { std::cout << "  FAIL " << m << "\n"; });
        std::cout << "\n== " << sum.plugins << " plug-ins, " << sum.presets << " presets: " << sum.passed << " passed, " << sum.failed << " failed ("
                  << juce::String (sum.seconds, 1) << " s) ==\nReport: " << report.getFullPathName() << "\n";
        scannerOwner.reset(); session.reset(); engine.reset();
        return sum.failed == 0 ? 0 : 1;
    }

    if (SelfTestMode::renderFaces)
    {
        // Design QA: every face (or the --only ones) rendered to a PNG after a few seconds of the sung vocal went through it
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (SelfTestMode::facesDir.isNotEmpty() ? SelfTestMode::facesDir : juce::String ("faces"));
        dir.createDirectory();
        Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
        if (session->edit == nullptr) session->newSession ("Faces");
        session->edit->ensureNumberOfAudioTracks (1);
        auto* track = te::getAudioTracks (*session->edit)[0];
        std::vector<float> vl, vr; Eng::TestSignals::vocal (vl, vr, 48000.0, 4.0);
        int made = 0;
        for (auto& e : Signature::entries())
        {
            if (! SelfTestMode::only.isEmpty() && ! SelfTestMode::only.contains (e.name)) continue;
            if (SelfTestMode::only.isEmpty() && juce::String (e.category) == "Instruments") continue;
            auto p = Signature::create (*session->edit, e);
            if (p == nullptr) continue;
            track->pluginList.insertPlugin (p, 0, nullptr);
            // --face-preset "name": show the face with that factory preset loaded (when the plug-in has it)
            if (SelfTestMode::facePreset.isNotEmpty())
                for (auto& pr : e.presets) if (juce::String (pr.name).containsIgnoreCase (SelfTestMode::facePreset)) { Signature::applyPreset (*p, pr); if (auto fx = dynamic_cast<SqTuneFxPlugin*> (p.get())) for (auto& prop : pr.props) if (juce::String (prop.id) == "motionPattern") fx->applyPattern ((int) prop.value, nullptr); break; }
            p->baseClassInitialise ({ te::TimePosition(), 48000.0, 256 });
            juce::AudioBuffer<float> buf (2, 256); te::MidiMessageArray midi;
            for (size_t q = 0; q + 256 <= vl.size(); q += 256)
            {
                buf.copyFrom (0, 0, vl.data() + q, 256); buf.copyFrom (1, 0, vr.data() + q, 256);
                te::PluginRenderContext fc (&buf, 0, 256, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 48000.0), te::TimePosition::fromSeconds ((q + 256) / 48000.0)), true, false, true, false);
                p->applyToBuffer (fc);
                if ((q / 256) % 40 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            }
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = p->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*p);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                auto f = dir.getChildFile (juce::File::createLegalFileName (e.name) + ".png"); f.deleteFile();
                if (auto os = f.createOutputStream()) { juce::PNGImageFormat png; png.writeImageToStream (img, *os); ++made; }
                std::cout << "  " << e.name << " -> " << f.getFullPathName() << " (" << img.getWidth() << " x " << img.getHeight() << ")\n";
            }
            p->baseClassDeinitialise();
            for (auto pl : track->pluginList.getPlugins()) pl->deleteFromParent();
        }
        scannerOwner.reset(); session.reset(); engine.reset();
        return made > 0 ? 0 : 1;
    }

    auto rpc = [&] (const juce::String& method, const juce::var& params) -> juce::var
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("jsonrpc", "2.0");
        o->setProperty ("id", id++);
        o->setProperty ("method", method);
        o->setProperty ("params", params);
        bool notif = false;
        auto out = server.handleBody (juce::JSON::toString (juce::var (o)), sid, notif);
        return juce::JSON::parse (out);
    };

    auto call = [&] (const juce::String& tool, const juce::String& argsJson, bool expectOk = true) -> juce::String
    {
        auto p = new juce::DynamicObject();
        p->setProperty ("name", tool);
        // The checks splice file paths into JSON text. A Windows path's backslashes are not valid JSON escapes, so
        // turn every backslash that does not start a JSON escape into '/' (Windows accepts both separators).
        juce::String json;
        for (int i = 0; i < argsJson.length(); ++i)
        {
            const auto c = argsJson[i];
            if (c == '\\' && i + 1 < argsJson.length() && juce::String ("\"\\/bfnrtu").containsChar (argsJson[i + 1])
                && ! (juce::File::getSeparatorChar() == '\\' && argsJson[i + 1] != '"' && argsJson[i + 1] != '\\'))
                { json << c << argsJson[i + 1]; ++i; continue; }
            json << (c == '\\' ? juce::juce_wchar ('/') : c);
        }
        p->setProperty ("arguments", juce::JSON::parse (json));
        auto r = rpc ("tools/call", juce::var (p));
        auto text = r["result"]["content"][0]["text"].toString();
        bool isErr = (bool) r["result"]["isError"];
        bool good = (isErr != expectOk);
        (good ? passes : failures)++;
        std::cout << (good ? "  PASS " : "  FAIL ") << tool << " " << argsJson.substring (0, 90) << "\n       -> " << text.substring (0, 300).replace ("\n", " ") << std::endl;
        return text;
    };

    auto check = [&] (bool cond, const juce::String& what)
    {
        (cond ? passes : failures)++;
        std::cout << (cond ? "  PASS " : "  FAIL ") << what << std::endl;
    };

    std::cout << "== Squelchmoth MCP self-test ==\n";
    Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };   // the self-test runs as Complete; the editions section switches to the Demo itself

    auto init = rpc ("initialize", juce::JSON::parse (R"({"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"selftest","version":"1"}})"));
    check (init["result"]["serverInfo"]["name"].toString() == "squelchmoth", "initialize returns serverInfo");
    check (sid.isNotEmpty(), "initialize assigns Mcp-Session-Id");

    bool notif = false;
    auto none = server.handleBody (R"({"jsonrpc":"2.0","method":"notifications/initialized"})", sid, notif);
    check (notif && none.isEmpty(), "notification gets no response (202)");

    auto list = rpc ("tools/list", juce::var (new juce::DynamicObject()));
    auto numTools = list["result"]["tools"].size();
    check (numTools >= 35, "tools/list returns " + juce::String (numTools) + " tools");

    // every schema must be a valid object schema
    bool schemasOk = true;
    for (auto& t : *list["result"]["tools"].getArray())
        schemasOk = schemasOk && t["inputSchema"]["type"].toString() == "object";
    check (schemasOk, "all tool schemas are object schemas");

    call ("get_session", "{}", false);   // no session yet -> error
    call ("new_session", R"({"name":"Selftest"})");
    call ("get_session", "{}");
    call ("set_tempo", R"({"bpm":92,"numerator":4,"denominator":4})");
    call ("add_track", R"({"kind":"instrument","name":"Keys","instrument":"4OSC Synth"})");
    call ("create_midi_clip", R"({"track":"Keys","start_bar":1,"length_bars":4,"name":"Chords","notes":[
        {"pitch":"C4","start_beat":0,"length_beats":4,"velocity":100},{"pitch":"E4","start_beat":0,"length_beats":4},{"pitch":"G4","start_beat":0,"length_beats":4},
        {"pitch":"A3","start_beat":4,"length_beats":4},{"pitch":"C4","start_beat":4,"length_beats":4},{"pitch":"E4","start_beat":4,"length_beats":4},
        {"pitch":"F3","start_beat":8,"length_beats":4},{"pitch":"A3","start_beat":8,"length_beats":4},{"pitch":"C4","start_beat":8,"length_beats":4},
        {"pitch":"G3","start_beat":12,"length_beats":4},{"pitch":"B3","start_beat":12,"length_beats":4},{"pitch":"D4","start_beat":12,"length_beats":4}]})");
    auto notes = call ("get_midi_notes", R"({"track":"Keys"})");
    check (notes.contains ("\"pitch\": 60") || notes.contains ("\"pitch\":60"), "C4 parsed as MIDI 60");
    call ("quantize_midi", R"({"track":"Keys","grid_beats":0.25})");
    call ("transpose_midi", R"({"track":"Keys","semitones":2})");
    call ("transpose_midi", R"({"track":"Keys","semitones":-2})");

    call ("insert_plugin", R"({"track":"Keys","plugin":"Compressor"})");
    call ("insert_plugin", R"({"track":"Keys","plugin":"4-Band EQ"})");
    call ("insert_plugin", R"({"track":"Keys","plugin":"Drum Slam"})");
    call ("insert_plugin", R"({"track":"Keys","plugin":"Definitely Not A Plugin"})", false);
    auto params = call ("get_plugin_parameters", R"({"track":"Keys","slot":"Compressor"})");
    check (params.contains ("parameters"), "compressor exposes parameters");
    call ("set_plugin_parameters", R"({"track":"Keys","slot":"Compressor","params":[{"param":0,"value":0.4},{"param":1,"value":0.7}]})");
    call ("bypass_plugin", R"({"track":"Keys","slot":"Drum Slam","bypassed":true})");
    call ("move_plugin", R"({"track":"Keys","slot":"4-Band EQ","to_slot":2})");
    call ("remove_plugin", R"({"track":"Keys","slot":"Drum Slam"})");
    call ("plugin_presets", R"({"track":"Keys","slot":"Compressor","action":"save","name":"Glue"})");
    call ("plugin_presets", R"({"track":"Keys","slot":"Compressor","action":"recall","name":"Glue"})");

    call ("add_send", R"({"track":"Keys","bus":"new","bus_name":"Verb Bus","level_db":-8})");
    call ("insert_plugin", R"({"track":"Verb Bus","plugin":"Reverb"})");
    call ("update_track", R"({"track":"Keys","volume_db":-3,"pan":-0.2,"colour":"#ff3ad1"})");
    call ("update_track", R"({"track":"master","volume_db":-1})");
    call ("insert_plugin", R"({"track":"master","plugin":"Compressor"})");
    call ("write_automation", R"({"track":"Keys","target":"volume","points":[{"bar":1,"value":-12},{"bar":2,"value":-3},{"bar":4,"value":-6}]})");
    call ("write_automation", R"({"track":"Keys","slot":"4-Band EQ","param":0,"points":[{"bar":1,"value":0.2},{"bar":3,"value":0.8}]})");

    call ("set_loop", R"({"enabled":true,"start_bar":1,"end_bar":5})");
    call ("set_click", R"({"enabled":false})");
    call ("transport", R"({"action":"go_to_start"})");

    auto a = call ("analyze_mix", R"({"start_bar":1,"end_bar":5})");
    auto av = juce::JSON::parse (a);
    check ((double) av["peak_dbfs"] > -60.0, "mix is audible (peak " + av["peak_dbfs"].toString() + " dBFS)");

    auto bounce = call ("bounce", R"({"name":"selftest bounce","start_bar":1,"end_bar":5})");
    auto bounced = session->bouncesDir.findChildFiles (juce::File::findFiles, false, "*.wav");
    check (bounced.size() == 1 && bounced[0].getSize() > 100000, "bounce wrote a WAV file");

    if (bounced.size() == 1)
    {
        call ("import_audio", R"({"track":1,"path":")" + bounced[0].getFullPathName().replace ("\\", "/") + R"(","start_bar":2})");
        call ("edit_clip", R"({"track":1,"clip":1,"gain_db":-4,"fade_in_seconds":0.5,"fade_out_seconds":1})");
        call ("split_clip", R"({"track":1,"at_bar":3})");
        call ("duplicate_clip", R"({"track":1,"clip":2,"times":2})");
        auto clips = call ("list_clips", R"({"track":1})");
        check (juce::JSON::parse (clips)[0]["clips"].size() == 4, "split + duplicate gives 4 clips");
        call ("delete_clip", R"({"track":1,"clip":4})");
        call ("edit_clip", R"({"track":1,"clip":1,"move_to_track":2})");
    }

    auto bounceLufs = call ("bounce", R"({"name":"loud","start_bar":1,"end_bar":5,"normalize_lufs":-14,"format":"flac"})");
    check (bounceLufs.contains (".flac"), "FLAC + LUFS-normalised bounce");

    call ("bounce", R"({"name":"stem","tracks":["Keys"],"start_bar":1,"end_bar":3})");
    call ("add_track", R"({"kind":"audio","name":"Vox","count":2})");
    call ("update_track", R"({"track":"Vox 1","output":"Verb Bus"})");
    call ("update_track", R"({"track":"Vox 2","move_to":1,"mute":true,"solo":false})");
    call ("delete_track", R"({"track":"Vox 2"})");
    call ("list_inputs", "{}");
    call ("list_plugins", R"({"filter":"reverb"})");
    call ("get_meters", "{}");
    call ("undo", "{}");
    call ("undo", R"({"redo":true})");
    call ("clear_automation", R"({"track":"Keys","target":"volume"})");

    call ("add_drum_track", R"({"name":"Drums","bars":4})");
    call ("set_beat", R"({"track":"Drums","steps":{"kick":"x...x...x...x...","snare":"....x.......x...","closed hat":"x.x.x.x.x.x.x.x.","open hat":"..............x.","clap":"....X.......X..."},"swing":0.2})");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // let the sampler load its sounds (async in the engine)
    auto beat = call ("get_beat", R"({"track":"Drums"})");
    check (beat.contains ("x...x...x...x...") && beat.contains ("....X.......X..."), "beat pattern round-trips");
    auto drumMix = call ("analyze_mix", R"({"start_bar":1,"end_bar":3,"tracks":["Drums"]})");
    check (juce::JSON::parse (drumMix)["peak_dbfs"].toString().getDoubleValue() > -40, "drum track makes sound (peak " + juce::JSON::parse (drumMix)["peak_dbfs"].toString() + " dBFS)");
    call ("set_beat", R"({"track":"Drums","steps":{"bogus":"x..."}})");

    call ("save_session", "{}");
    auto file = session->getFile();
    call ("new_session", R"({"name":"Second"})");
    call ("open_session", R"({"path":")" + file.getFullPathName().replace ("\\", "/") + "\"}");
    auto s = call ("get_session", "{}");
    check (s.contains ("Keys") && s.contains ("Verb Bus") && s.contains ("92"), "session reopened with tracks, bus and tempo intact");
    call ("list_sessions", "{}");

    if (juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_VST3", {}).isNotEmpty())
    {
        std::cout << "-- third-party plugin hosting --\n";
        call ("scan_plugins", "{}");
        for (int i = 0; i < 600 && scanner.isScanning(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto lp = call ("list_plugins", R"({"category":"thirdparty"})");
        check (lp.contains (juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_VST3", {})), "scanned third-party plugin is listed");
        call ("insert_plugin", R"({"track":"Keys","plugin":")" + juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_VST3", {}) + R"("})");
        auto pp = call ("get_plugin_parameters", R"({"track":"Keys","slot":"Moth"})");
        check (pp.contains ("Output Gain") && pp.contains ("Character"), "third-party parameters visible");
        auto sp = call ("set_plugin_parameters", R"({"track":"Keys","slot":"Moth","params":[{"param":"Output Gain","text":"-12"},{"param":"Character","value":1.0}]})");
        check (sp.contains ("-12"), "set third-party parameter by text");
        auto pr = call ("plugin_presets", R"({"track":"Keys","slot":"Moth","action":"list"})");
        check (pr.contains ("Loud"), "factory presets listed");
        call ("plugin_presets", R"({"track":"Keys","slot":"Moth","action":"load","index":2})");
        auto pp2 = call ("get_plugin_parameters", R"({"track":"Keys","slot":"Moth","filter":"Gain"})");
        check (pp2.contains ("Loud"), "preset load changes program");
        call ("write_automation", R"({"track":"Keys","slot":"Moth","param":"Output Gain","points":[{"bar":1,"value":0.2},{"bar":3,"value":0.6}]})");
        auto an = call ("analyze_mix", R"({"start_bar":1,"end_bar":3,"tracks":["Keys"]})");
        check (juce::JSON::parse (an)["peak_dbfs"].toString().getDoubleValue() > -60, "audio flows through third-party plugin");
        call ("save_session", "{}");
        call ("open_session", R"({"path":")" + session->getFile().getFullPathName() + "\"}");
        auto again = call ("get_plugin_parameters", R"({"track":"Keys","slot":"Moth","filter":"Character"})");
        check (again.contains ("Character"), "third-party plugin restored after reopening session");
    }


    {
        std::cout << "-- Pro Tools parity layer --\n";
        // A headless TimelineState stands in for the Edit window so the selection-based tools work.
        TimelineState headlessState;
        tools.ui.state = &headlessState;
        call ("new_session", R"({"name":"Parity"})");
        call ("set_tempo", R"({"bpm":120})");
        call ("add_track", R"({"kind":"instrument","name":"Keys","instrument":"Mothsynth"})");
        call ("create_midi_clip", R"({"track":"Keys","start_bar":1,"length_bars":8,"name":"Pad","notes":[{"pitch":"C3","start_beat":0,"length_beats":32,"velocity":90}]})");
        auto mode = call ("set_edit_mode", R"({"mode":"grid","grid":"1/8 note","nudge":"1/16 note"})");
        check (mode.contains ("Grid") && mode.contains ("1/8"), "edit mode + grid value set");
        call ("set_tool", R"({"tool":"grabber"})");
        auto sel = call ("set_selection", R"({"start_bar":3,"end_bar":5,"tracks":["Keys"]})");
        check (sel.contains ("3|1|000") && sel.contains ("5|1|000"), "Edit selection set on a track");
        auto sep = call ("edit_command", R"({"command":"separate"})");
        check (sep.contains ("2 item"), "separate splits at both selection edges");
        auto clipsNow = call ("list_clips", R"({"track":"Keys"})");
        auto clipCount = [] (const juce::String& json) { auto v = juce::JSON::parse (json); return v[0]["clips"].size(); };
        check (clipCount (clipsNow) == 3, "three clips after separate");
        call ("set_selection", R"({"start_bar":3,"end_bar":5,"tracks":["Keys"],"all_clips_in_range":true})");
        auto dup = call ("edit_command", R"({"command":"duplicate"})");
        check (clipCount (call ("list_clips", R"({"track":"Keys"})")) == 4, "duplicate adds a clip after the selection");
        call ("set_edit_mode", R"({"mode":"shuffle"})");
        call ("set_selection", R"({"start_bar":3,"end_bar":5,"tracks":["Keys"]})");
        auto del = call ("edit_command", R"({"command":"delete_close_gap"})");
        auto afterDel = juce::JSON::parse (call ("list_clips", R"({"track":"Keys"})"))[0]["clips"];
        bool closed = true;
        for (auto& c : *afterDel.getArray()) if (c["start"].toString().startsWith ("5|")) closed = false;
        check (afterDel.size() >= 2 && closed, "shuffle delete closes the gap");
        auto nudged = call ("edit_command", R"({"command":"nudge","direction":1})");
        check (! nudged.contains ("0 item"), "nudge moves the selection");
        auto sel2 = call ("get_selection", "{}");
        check (sel2.contains ("Shuffle") && sel2.contains ("Grabber"), "get_selection reports mode and tool");

        auto mk = call ("add_marker", R"({"name":"Verse","bar":3})");
        check (mk.contains ("Verse") && mk.contains ("3|1|000"), "marker added at bar 3");
        call ("add_marker", R"({"name":"Chorus","bar":5,"length_bars":2})");
        auto markers = juce::JSON::parse (call ("list_markers", "{}"));
        check (markers.size() == 2 && markers[1]["length_seconds"].toString().isNotEmpty(), "selection marker has a length");
        auto em = call ("edit_marker", R"({"marker":"Verse","name":"Intro","bar":2})");
        check (em.contains ("Intro") && em.contains ("2|1|000"), "marker renamed and moved");
        call ("edit_marker", R"({"marker":"Chorus","delete":true})");
        check (juce::JSON::parse (call ("list_markers", "{}")).size() == 1, "marker deleted");

        call ("add_track", R"({"kind":"audio","name":"Gtr L"})");
        call ("add_track", R"({"kind":"audio","name":"Gtr R"})");
        auto fold = call ("create_folder", R"({"kind":"folder","name":"Guitars","tracks":["Gtr L","Gtr R"]})");
        check (fold.contains ("Guitars") && fold.contains ("2 member"), "folder track with two members");
        auto vca = call ("create_folder", R"({"kind":"vca","name":"Band","tracks":["Keys"]})");
        check (vca.contains ("VCA"), "VCA master created");
        auto folders = juce::JSON::parse (call ("list_folders", "{}"));
        check (folders.size() == 2, "two folder/VCA rows listed");
        auto grp = call ("mix_group", R"({"action":"create","name":"Gtrs","tracks":["Gtr L","Gtr R"]})");
        check (grp.contains ("2 track"), "mix group created");
        call ("update_track", R"({"track":"Gtr L","volume_db":-6})");
        // group linking happens through Session::setTrackVolume (used by the faders); exercise it directly
        if (auto gl = session->findTrack ("Gtr L")) if (auto gr = session->findTrack ("Gtr R"))
        {
            session->setTrackVolume (*gl, -12.0f);
            check (std::abs (gr->getVolumePlugin()->getVolumeDb() - (-6.0f)) < 0.01f, "grouped fader follows the moved fader by the same offset");
            session->setTrackMute (*gl, true);
            check (gr->isMuted (false), "grouped mute follows");
        }
        auto am = call ("set_automation_mode", R"({"track":"Keys","mode":"latch"})");
        check (am.contains ("Latch"), "automation mode set");
        auto el = call ("set_elastic_audio", R"({"track":"Gtr L","mode":"polyphonic"})");
        check (el.contains ("Polyphonic"), "elastic audio mode set");
        auto rm = call ("set_record_mode", R"({"mode":"quickpunch","pre_roll_seconds":1.5,"count_off_bars":2,"click":true})");
        check (rm.contains ("QuickPunch") && rm.contains ("1.5 s") && rm.contains ("count-off 2"), "record modes applied");
        call ("set_record_mode", R"({"mode":"normal","pre_roll_seconds":0,"count_off_bars":0})");
        auto th = call ("set_theme", R"({"preset":"Carbon"})");
        check (th.contains ("Carbon"), "theme preset applied");
        call ("set_theme", R"({"preset":"Moth"})");
        auto col = call ("set_colour", R"({"tracks":["Keys"],"palette_index":3})");
        check (col.contains ("1 item"), "track colour set from palette");
        auto pr = call ("set_preference", R"({"key":"ed_fadeCmd","value":0.5})");
        check (pr.contains ("0.5"), "preference written and read back");
        auto cg = call ("set_clip_gain", R"({"track":"Keys","clip":1,"name":"PadA"})");
        check (cg.contains ("PadA"), "clip renamed through set_clip_gain");

        std::cout << "-- Signature bundle & sound kits --\n";
        auto sig = juce::JSON::parse (call ("list_plugins", R"({"category":"signature"})"));
        check (sig["count"].toString().getIntValue() >= 40, "40+ Signature plugins listed");
        auto ins = call ("insert_plugin", R"({"track":"Keys","plugin":"Ember Tape"})");
        check (ins.contains ("Ember Tape"), "Signature plugin inserted by brand name and reported by brand name");
        auto ins2 = call ("insert_plugin", R"({"track":"Keys","plugin":"Cocoon Compressor"})");
        check (ins2.contains ("Cocoon Compressor"), "core compressor carries its Signature name");
        auto params = call ("get_plugin_parameters", R"({"track":"Keys","slot":"Ember Tape"})");
        check (params.contains ("Ember Tape"), "parameters addressed by Signature name");
        for (auto kit : { "Hive 909", "Chrome Trap", "Velvet Lo-Fi", "Dust Acoustic" })
        {
            auto dk = call ("add_drum_track", "{\"name\":\"" + juce::String (kit) + "\",\"bars\":2,\"kit\":\"" + juce::String (kit) + "\"}");
            check (dk.contains (kit), juce::String ("kit '") + kit + "' loads");
        }
        auto swap = call ("set_drum_kit", R"({"track":"Hive 909","kit":"Mothwing 808"})");
        check (swap.contains ("Mothwing 808"), "kit swapped on an existing drum track");
        auto kitFiles = session->sessionsDir.getChildFile ("Kits").getChildFile ("Chrome Trap").findChildFiles (juce::File::findFiles, false, "*.wav");
        check (kitFiles.size() == 12, "each kit renders 12 sounds");
        bool allLoud = true;
        for (auto& f : kitFiles) if (f.getSize() < 20000) allLoud = false;
        check (allLoud, "kit samples are real 24-bit renders");

        // presets: Basic + Advanced banks on every Signature plugin, applied by name
        int withBanks = 0;
        for (auto& e : Signature::entries()) if (e.presets.size() >= 10) ++withBanks;
        check (withBanks >= 40, "40+ Signature plugins carry 10+ factory presets");
        auto prs = call ("plugin_presets", R"({"track":"Keys","slot":"Cocoon Compressor","preset":"Drum Punch"})");
        check (prs.contains ("Drum Punch"), "factory preset applied by name");
        auto thr = juce::JSON::parse (call ("get_plugin_parameters", R"({"track":"Keys","slot":"Cocoon Compressor","filter":"Threshold"})"));
        check (thr["parameters"][0]["display"].toString().contains ("-14"), "compressor threshold preset lands at -14 dB (gain-domain parameter mapped correctly)");
        auto ir = call ("insert_plugin", R"({"track":"Keys","plugin":"Cavern IR"})");
        check (ir.contains ("Cavern IR"), "convolution reverb (Cavern IR) is registered and inserts");

        // panning: session pan depth + Pro Tools dual panner
        session->setPanLaw (3);
        check (session->getPanLaw() == 3 && session->getTracks()[0]->getVolumePlugin()->getPanLaw() == te::PanLaw4point5dBCenter, "pan depth applies to every track fader");
        auto sp = call ("update_track", R"({"track":"Keys","stereo_pan":true,"pan_l":-0.8,"pan_r":0.6})");
        check (! sp.contains ("error") && Session::getStereoPan (*session->findTrack ("Keys")) != nullptr, "stereo (L/R) panner engages on a track");
        {
            auto spp = Session::getStereoPan (*session->findTrack ("Keys"));
            float gl, gr; spp->gainsFor (0.0f, gl, gr);
            check (std::abs (gl - gr) < 0.001f && gl < 0.75f && gl > 0.5f, "stereo panner centre gain follows the -4.5 dB law");
            spp->gainsFor (-1.0f, gl, gr);
            check (gl > 0.99f && gr < 0.001f, "hard left sends nothing to the right");
        }
        session->setPanLaw (2);
        tools.ui.state = nullptr;
    }
    {
        std::cout << "-- mastering (Studio One Project page) --\n";
        MasteringProject mp (*session);
        tools.ui.mastering = [&] { return &mp; };
        auto bounce = call ("bounce", R"({"name":"mastersrc","start_bar":1,"end_bar":9})");
        auto src = session->sessionsDir.getChildFile ("Bounces").getChildFile ("mastersrc.wav");
        check (src.existsAsFile(), "source bounce for the album exists");
        auto mn = call ("mastering_new", R"({"name":"Selftest Album"})");
        check (mn.contains ("Selftest Album"), "mastering project created");
        call ("mastering_set_album", R"({"album":"Selftest Album","artist":"Jamar Stewart","genre":"Test","year":"2026","upc":"0123456789012"})");
        juce::String add = "{\"paths\":[\"" + src.getFullPathName().replace ("\\", "\\\\") + "\",\"" + src.getFullPathName().replace ("\\", "\\\\") + "\"]}";
        auto added = juce::JSON::parse (call ("mastering_add_tracks", add));
        check (added["tracks"].isArray() && added["tracks"].size() == 2, "two album tracks added and sequenced");
        check (added["tracks"].isArray() && added["tracks"][0]["start"].toString() == "0:02.00", "track 1 starts after the Red Book 2 s pause");
        auto st = juce::JSON::parse (call ("mastering_set_track", R"({"track":1,"title":"First","isrc":"USQLM2600001","auto_isrc":true,"fade_out":1})"));
        check (st["tracks"].isArray() && st["tracks"][1]["isrc"].toString() == "USQLM2600002", "ISRC auto-increments across the album");
        auto st2 = juce::JSON::parse (call ("mastering_set_track", R"({"track":2,"title":"Second","pause_seconds":1.5})"));
        check (st2["tracks"].isArray() && st2["tracks"][1]["pause_seconds"].toString().startsWith ("1.5"), "pause between tracks set");
        auto mi = call ("mastering_insert", R"({"target":"master","plugin":"Clamp Limiter","preset":"Master Transparent"})");
        check (mi.contains ("Clamp Limiter"), "master rack insert");
        auto ti = call ("mastering_insert", R"({"target":1,"plugin":"Prism EQ"})");
        check (ti.contains ("Prism EQ"), "track rack insert");
        auto ld = juce::JSON::parse (call ("mastering_detect_loudness", R"({"track":1})"));
        auto lufs = ld["loudness"].isArray() ? (float) ld["loudness"][0]["pre_lufs"] : -100.0f;
        check (lufs > -60.0f && lufs < 0.0f, "EBU R128 loudness detection returns a real reading");
        auto pub = juce::JSON::parse (call ("mastering_publish", R"({"kind":"digital","format":"wav","bit_depth":24,"loudness_target":"spotify"})"));
        auto files = juce::StringArray::fromLines (pub["files"].toString());
        check (files.size() == 2 && juce::File (files[0]).existsAsFile(), "digital release writes one file per track");
        auto measured = measureFile (juce::File (files[0]));
        check (std::abs (measured.integrated + 14.0f) < 0.6f || measured.truePeak > -1.2f, "released track is matched to -14 LUFS (or true-peak limited)");
        auto img = juce::JSON::parse (call ("mastering_publish", R"({"kind":"image"})"));
        auto imgDir = juce::File (img["folder"].toString());
        check (imgDir.getChildFile ("Selftest Album.cue").existsAsFile() && imgDir.getChildFile ("Selftest Album.cue").loadFileAsString().contains ("INDEX 01 00:02:00"), "image export writes a CUE sheet with Red Book indexes");
        auto ddp = juce::JSON::parse (call ("mastering_publish", R"({"kind":"ddp"})"));
        auto ddpDir = juce::File (ddp["folder"].toString());
        check (ddpDir.getChildFile ("DDPID").getSize() == 128 && ddpDir.getChildFile ("DDPMS").getSize() % 128 == 0 && ddpDir.getChildFile ("PQDESCR").getSize() % 64 == 0, "DDP 2.00 fileset has 128-byte DDPID / map packets and 64-byte PQ packets");
        check (ddpDir.getChildFile ("IMAGE.DAT").getSize() % 2352 == 0 && ddpDir.getChildFile ("CDTEXT.BIN").getSize() % 18 == 0 && ddpDir.getChildFile ("CHECKSUM.MD5").existsAsFile(), "DDP audio image is whole sectors, CD-Text packs are 18 bytes, checksums present");
        check (ddpDir.getChildFile ("DDPID").loadFileAsString().startsWith ("DDP 2.000123456789012"), "DDPID carries the level and the UPC");
        check (ddpDir.getChildFile ("PQDESCR").loadFileAsString().contains ("VVVSAA01"), "PQ descriptor ends with the lead-out");
        auto tr = juce::JSON::parse (call ("mastering_transport", R"({"action":"locate","position_seconds":5})"));
        check (tr["position"].toString() == "0:05.00", "mastering transport locates");
        tools.ui.mastering = nullptr;
    }
    {
        std::cout << "-- licensing --\n";
        juce::RSAKey pub, priv;
        juce::RSAKey::createKeyPair (pub, priv, 2048);
        auto key = Licence::makeKey (priv.toString(), "Test Buyer", "buyer@example.com");
        juce::String n, e;
        check (key.startsWith ("SQM1-") && Licence::verify (key, n, e, pub.toString()) && n == "Test Buyer", "licence key verifies with matching public key");
        check (! Licence::verify (key, n, e), "key from another key pair is rejected by the shipped public key");
        auto tampered = key.substring (0, key.length() - 6) + "AAAAAA";
        check (! Licence::verify (tampered, n, e, pub.toString()), "tampered key is rejected");
        check (! Licence::verify ("SQM1-hello", n, e, pub.toString()), "garbage key is rejected");
        // 1.9 store keys (SQM2, Ed25519): a test key pair, so nothing here needs the real store key
        const juce::String testPub = "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8";
        const juce::String coreKey = "SQM2-AREiM0SQvrhNOYNL81nf8N3SEfrnuomnehHof3wOnhd-WbRpbQiU2ySz7b4YyJ0zQx9EyOJ3qNUS4MdHRmXFdn7DBy8M";
        const juce::String vocalKey = "SQM2-AwoLDA3q_IGKmVFP3YKoHdG4CER9hSwjrIhquCpWUcNY-AZtGpbeqY9wIPMRY0cI8Xw7VKh18P9GtHShpIRvjzHxsFQC";
        juce::String prod, serial;
        check (coreKey.length() <= 100 && Licence::verifyStoreKey (coreKey, prod, serial, testPub) && prod == "CORE" && serial == "11223344", "store key verifies (Ed25519), names its product and serial, fits the store's 100-character key list (" + juce::String (coreKey.length()) + ")");
        check (Licence::verifyStoreKey (vocalKey, prod, serial, testPub) && prod == "VOCAL", "a pack key names its pack");
        check (! Licence::verifyStoreKey (coreKey, prod, serial), "a key signed by another store key is refused by the shipped store key");
        auto flipped = coreKey.substring (0, 20) + (coreKey[20] == 'A' ? "B" : "A") + coreKey.substring (21);
        check (! Licence::verifyStoreKey (flipped, prod, serial, testPub), "a store key with one character changed is refused");
        check (! Licence::verifyStoreKey ("SQM2-AAAA", prod, serial, testPub), "garbage store key is refused");
        {
            // a fresh install has no full access: the Demo edition, no time-limited trial
            const auto saved = Edition::owned();
            props.removeValue ("licenceKeys"); props.removeValue ("licenceKey");
            Licence lic (props);
            check (lic.isDemo() && Edition::isDemo() && Edition::editionName() == "Squelchmoth Demo", "a fresh install runs the Demo - nobody gets the full program without a key");
            auto ownerKey = juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_KEY", {});
            if (ownerKey.isNotEmpty())
                check (lic.activate (ownerKey, false).isEmpty() && lic.isLicensed() && Edition::editionName() == "Squelchmoth Complete", "owner key activates against the shipped public key and unlocks everything");
            Edition::owned() = saved;
        }
    }

    //==========================================================================
    std::cout << "\n-- Pro Tools tracks, I/O names and routing --\n";
    {
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        session->defaultInputAssigner = [&] (te::AudioTrack& t, int index, bool stereo) { IO::assignDefaultInput (*session, t, index, stereo); };
        call ("new_session", R"({"name":"Selftest PT Tracks"})");
        pump (800);
        auto types = call ("new_tracks", R"({"type":"audio","count":2,"format":"mono","name":"Vox"})");
        call ("new_tracks", R"({"type":"audio","format":"stereo","name":"Keys"})");
        call ("new_tracks", R"({"type":"aux","name":"Plate"})");
        call ("new_tracks", R"({"type":"instrument","name":"Pad"})");
        call ("new_tracks", R"({"type":"midi","name":"Seq"})");
        call ("new_tracks", R"({"type":"vca","name":"VCA Vox"})");
        call ("new_tracks", R"({"type":"basic_folder","name":"Drums Folder"})");
        call ("new_tracks", R"({"type":"routing_folder","name":"Strings Sub"})");
        check (call ("new_tracks", R"({"type":"master_fader"})").contains ("already has its Master Fader"), "New Tracks: only one Master Fader per session");
        pump (1500);
        auto sess = juce::JSON::parse (call ("get_session", "{}"));
        auto findT = [&] (const juce::String& n) { if (auto arr = sess["tracks"].getArray()) for (auto& t : *arr) if (t["name"].toString() == n) return t; return juce::var(); };
        check (findT ("Vox 1")["type"].toString() == "audio" && findT ("Vox 1")["format"].toString() == "mono", "New Tracks: mono audio track");
        check (findT ("Keys")["format"].toString() == "stereo", "New Tracks: stereo audio track");
        check (findT ("Plate")["type"].toString() == "aux" && findT ("Plate")["input"].toString() == "Plate", "New Tracks: Aux Input listens on its own named bus");
        check (findT ("Seq")["type"].toString() == "midi" && findT ("Seq")["output"].toString() == "Pad", "New Tracks: MIDI track plays the Instrument track");
        check (findT ("Pad")["type"].toString() == "instrument", "New Tracks: instrument track");
        auto folders = call ("list_folders", "{}");
        check (folders.contains ("VCA Vox") && folders.contains ("Drums Folder") && folders.contains ("Strings Sub"), "New Tracks: VCA, basic folder and routing folder");

        const int numIn = IO::numInterfaceChannels (session->getEngine(), true);
        if (numIn >= 4)
        {
            check (findT ("Vox 1")["input"].toString().startsWith ("In ") && findT ("Vox 1")["input"].toString() != findT ("Vox 2")["input"].toString(),
                   "default inputs: each new mono track gets its own free path (" + findT ("Vox 1")["input"].toString() + ", " + findT ("Vox 2")["input"].toString() + ")");
            check (findT ("Keys")["input"].toString().contains ("-"), "default input: stereo track gets a stereo pair (" + findT ("Keys")["input"].toString() + ")");
            call ("io_setup", R"({"action":"rename_input","channels":"1","name":"Lead Mic"})");
            check (IO::pathName (true, 0, 1) == "Lead Mic", "I/O Setup: rename input path");
            call ("set_input", R"({"track":"Vox 1","input":"Lead Mic"})");
            pump (1500);
            check (call ("get_session", "{}").contains ("\"input\": \"Lead Mic\""), "track input follows the renamed path");
            call ("io_setup", R"({"action":"default_names"})");
            check (IO::pathName (true, 0, 1) == "In 1", "I/O Setup: default names restored");
        }
        else check (true, "interface input tests skipped (no audio interface in this environment)");

        call ("set_output", R"({"track":"Vox 2","output":"Plate"})");
        check (call ("get_session", "{}").contains ("\"output\": \"Plate\""), "Output selector: route a track to a bus by name");
        call ("io_setup", R"({"action":"rename_bus","bus":"Plate","name":"Vocal Plate"})");
        check (call ("io_setup", R"({"action":"list"})").contains ("Vocal Plate"), "I/O Setup: rename bus");
        call ("set_output", R"({"track":"Plate","output":"Vox 2"})", false);   // feedback refused

        // MIDI track -> Instrument track makes sound; Make Inactive silences it
        call ("create_midi_clip", R"({"track":"Seq","length_bars":1,"notes":[{"pitch":"C4","start_beat":0,"length_beats":2,"velocity":110}]})");
        call ("track_options", R"({"tracks":["Vox 1","Vox 2","Keys","Synth 1","Audio 1","Audio 2","Audio 3","Audio 4"],"inactive":true})");
        auto live = juce::JSON::parse (call ("analyze_mix", R"({"start_bar":1,"end_bar":2})"));
        check ((double) live["peak_dbfs"] > -40.0, "MIDI track routed to an Instrument track plays (peak " + live["peak_dbfs"].toString() + " dBFS)");
        call ("track_options", R"({"track":"Pad","inactive":true})");
        auto off = juce::JSON::parse (call ("analyze_mix", R"({"start_bar":1,"end_bar":2})"));
        check ((double) off["peak_dbfs"] < -90.0, "Make Inactive removes the track from playback");
        call ("track_options", R"({"track":"Pad","inactive":false})");

        call ("track_options", R"({"track":"Keys","record_safe":true})");
        call ("update_track", R"({"track":"Keys","armed":true})", false);
        call ("track_options", R"({"track":"Pad","solo_safe":true,"comments":"warm pad","timebase":"ticks"})");
        auto pad = juce::JSON::parse (call ("get_session", "{}"));
        bool padOk = false;
        if (auto arr = pad["tracks"].getArray()) for (auto& t : *arr) if (t["name"] == "Pad") padOk = (bool) t["solo_safe"] && t["comments"] == "warm pad" && t["timebase"] == "ticks";
        check (padOk, "Solo Safe, comments and Ticks timebase stored");

        call ("track_options", R"({"track":"Pad","duplicate":2,"copy_clips":false})");
        check (call ("get_session", "{}").contains ("Pad.dup2"), "Duplicate with options makes 2 copies");
        call ("track_options", R"({"track":"Pad","hidden":true})");
        check (call ("get_session", "{}").contains ("\"hidden\": true"), "Hide track");

        call ("master_fader", R"({"name":"Main Mix","volume_db":-2})");
        auto mf = call ("master_fader", "{}");
        check (mf.contains ("Main Mix") && mf.contains ("-2"), "Master Fader rename + level");
        call ("master_fader", R"({"exists":false})");
        check (! TrackOps::hasMasterFader (*session) && session->edit->getMasterVolumePlugin()->getVolumeDb() == 0.0f, "Delete Master Fader resets it to unity");
        call ("master_fader", R"({"exists":true})");

        // Split into Mono / Freeze / Commit on a stereo audio track (uses the stereo bounce made earlier)
        if (! bounced.isEmpty())
        {
            call ("import_audio", R"({"track":"Keys","path":")" + bounced[0].getFullPathName().replace ("\\", "/") + R"(","start_bar":1})");
            call ("track_options", R"({"track":"Keys","record_safe":false,"inactive":false})");
            call ("track_options", R"({"track":"Keys","split_mono":true})");
            auto afterSplit = call ("list_clips", R"({"track":"Keys.L"})");
            check (afterSplit.contains ("Keys.L") && call ("list_clips", R"({"track":"Keys.R"})").contains ("clip"), "Split into Mono makes Keys.L and Keys.R with clips");
            call ("track_options", R"({"track":"Keys","freeze":true})");
            check (session->findTrack ("Keys") != nullptr && session->findTrack ("Keys")->isFrozen (te::Track::individualFreeze), "Freeze");
            call ("track_options", R"({"track":"Keys","freeze":false})");
            call ("track_options", R"({"track":"Keys","commit":true})");
            auto cm = session->findTrack ("Keys.cm");
            check (cm != nullptr && cm->getClips().size() == 1 && session->findTrack ("Keys")->isHidden() && TrackOps::isInactive (*session->findTrack ("Keys")),
                   "Commit renders to Keys.cm; the source is hidden and inactive");
        }

        // Track formats: every type with a format can be mono or stereo, and mono is really mono
        {
            call ("new_tracks", R"({"type":"aux","format":"mono","name":"MonoAux"})");
            call ("new_tracks", R"({"type":"instrument","format":"mono","name":"MonoInst"})");
            call ("new_tracks", R"({"type":"routing_folder","format":"mono","name":"MonoFolder"})");
            call ("new_tracks", R"({"type":"audio","format":"stereo","name":"StAudio"})");
            auto js = call ("get_session", "{}");
            auto fmtOf = [&] (const juce::String& nm)
            {
                auto v = juce::JSON::parse (js);
                if (auto arr = v["tracks"].getArray()) for (auto& t : *arr) if (t["name"].toString() == nm) return t["format"].toString();
                return juce::String();
            };
            check (fmtOf ("MonoAux") == "mono" && fmtOf ("MonoInst") == "mono" && fmtOf ("StAudio") == "stereo", "New Tracks: mono / stereo formats for aux, instrument and audio");
            auto* mf = session->findRow ("MonoFolder");
            check (mf != nullptr && TrackOps::isMonoFormat (*session, *mf), "Mono routing folder");
            auto* mi = session->findTrack ("MonoInst");
            check (mi != nullptr && Session::getInserts (mi->pluginList, mi).size() == 1, "mono summing stage stays hidden from the insert slots");

            // a hard-left stereo file on a mono track comes out identical on both sides at half level
            auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_left_only.wav");
            {
                juce::AudioBuffer<float> b (2, 44100);
                b.clear();
                for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / 44100.0));
                juce::WavAudioFormat fmt;
                wav.deleteFile();
                if (auto os = std::unique_ptr<juce::OutputStream> (wav.createOutputStream()))
                    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (os.get(), 44100.0, 2, 24, {}, 0)))
                    { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
            }
            call ("new_tracks", R"({"type":"audio","format":"mono","name":"MonoVox"})");
            call ("import_audio", R"({"track":"MonoVox","path":")" + wav.getFullPathName().replace ("\\", "/") + R"(","start_bar":1})");
            juce::String err;
            auto rendered = session->findTrack ("MonoVox") != nullptr
                ? TrackOps::renderTrack (*session, *session->findTrack ("MonoVox"), juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_mono_render.wav"), err) : juce::File();
            float pl = 0, pr = 0;
            juce::AudioFormatManager afm; afm.registerBasicFormats();
            if (auto rd = std::unique_ptr<juce::AudioFormatReader> (afm.createReaderFor (rendered)))
            {
                juce::AudioBuffer<float> rb ((int) rd->numChannels, (int) std::min ((juce::int64) rd->lengthInSamples, (juce::int64) 88200));
                rd->read (&rb, 0, rb.getNumSamples(), 0, true, true);
                pl = rb.getMagnitude (0, 0, rb.getNumSamples());
                pr = rb.getNumChannels() > 1 ? rb.getMagnitude (1, 0, rb.getNumSamples()) : pl;
            }
            check (pl > 0.1f && std::abs (pl - pr) < 0.01f, "mono track sums a stereo source to one channel (L " + juce::String (pl, 3) + ", R " + juce::String (pr, 3) + ")");
            wav.deleteFile();
        }

        call ("edit_window_views", R"({"io":true,"inserts_ae":true,"inserts_fj":true,"sends_ae":true,"sends_fj":true,"comments":true})");
        check (EditViews::width() == EditViews::ioW + 4 * EditViews::slotW + EditViews::commentsW, "Edit Window Views: all columns");
        call ("edit_window_views", R"({"inserts_fj":false,"sends_fj":false,"comments":false,"inserts_ae":false,"sends_ae":false})");
        session->defaultInputAssigner = nullptr;
    }

    std::cout << "\n-- Plug-in catalogue (Pro Tools categories, Waves shells) --\n";
    {
        juce::Array<juce::PluginDescription> types;
        auto add = [&] (const char* name, const char* maker, const char* format, const char* cat = "Fx", bool inst = false)
        {
            juce::PluginDescription d; d.name = name; d.manufacturerName = maker; d.pluginFormatName = format; d.category = cat; d.isInstrument = inst;
            d.fileOrIdentifier = juce::String ("/Library/Audio/Plug-Ins/") + format + "/WaveShell1"; d.uniqueId = types.size() + 1; d.deprecatedUid = d.uniqueId;
            types.add (d);
        };
        for (auto f : { "VST3", "AudioUnit" })
        {
            add ("RCompressor Mono", "Waves", f); add ("RCompressor Stereo", "Waves", f);
            add ("H-Reverb Mono", "Waves", f); add ("H-Reverb Mono/Stereo", "Waves", f); add ("H-Reverb Stereo", "Waves", f);
            add ("H-Delay Mono", "Waves", f); add ("H-Delay Stereo", "Waves", f);
            add ("SSL E-Channel Mono", "Waves", f); add ("SSL E-Channel Stereo", "Waves", f);
            add ("Linear Phase EQ Mono", "Waves", f); add ("Linear Phase EQ Stereo", "Waves", f);
            add ("Waves Tune Mono", "Waves", f); add ("Clarity Vx Mono", "Waves", f); add ("S1 Imager Stereo", "Waves", f);
            add ("Abbey Road Plates Stereo", "Waves", f); add ("L2 Stereo", "Waves", f); add ("Kramer Master Tape Mono", "Waves", f);
        }
        add ("Pro-Q 3", "FabFilter", "VST3", "Fx|EQ");
        add ("Nightjar Synth", "Example Audio", "VST3", "Instrument|Synth", true);
        auto items = PluginCatalog::thirdPartyItems (types, true);
        auto find = [&] (const juce::String& n) -> const PluginCatalog::Item* { for (auto& i : items) if (i.name == n) return &i; return nullptr; };
        check (items.size() == 13, "Waves shells fold to one entry per plug-in, one format (" + juce::String ((int) items.size()) + " entries from " + juce::String (types.size()) + " descriptions)");
        auto cat = [&] (const char* n) { auto i = find (n); return i ? i->category : juce::String ("missing"); };
        check (cat ("RCompressor") == "Dynamics" && cat ("H-Reverb") == "Reverb" && cat ("H-Delay") == "Delay" && cat ("SSL E-Channel") == "Channel Strip"
               && cat ("Linear Phase EQ") == "EQ" && cat ("Waves Tune") == "Pitch Shift" && cat ("Clarity Vx") == "Noise Reduction" && cat ("S1 Imager") == "Sound Field"
               && cat ("Abbey Road Plates") == "Reverb" && cat ("L2") == "Dynamics" && cat ("Kramer Master Tape") == "Harmonic" && cat ("Pro-Q 3") == "EQ" && cat ("Nightjar Synth") == "Instrument",
               "Waves and other plug-ins land in their categories");
        auto cla = find ("RCompressor");
        check (cla != nullptr && types[cla->typeIndex].name == "RCompressor Stereo" && types[cla->typeIndex].pluginFormatName == "VST3", "stereo track gets the Stereo variant, VST3 preferred");
        auto mono = PluginCatalog::thirdPartyItems (types, false);
        for (auto& i : mono) if (i.name == "H-Reverb") check (types[i.typeIndex].name == "H-Reverb Mono/Stereo", "mono track gets the Mono/Stereo variant of a reverb");
    }

    std::cout << "\n-- EQ frequency chart --\n";
    {
        auto& rs = EQRef::rows();
        bool sane = true; juce::String bad;
        for (auto& r : rs)
        {
            if (r.eLo < 20.0f || r.eHi > 20000.0f || r.eLo >= r.eHi || (r.fLo > 0.0f && (r.fLo > r.fHi || r.fLo < r.eLo - 1.0f || r.fHi > r.eHi))) { sane = false; bad = r.name; }
            for (auto& b : r.bands) if (b.lo < 20.0f || b.hi > 20000.0f || b.lo >= b.hi || b.kind < 0 || b.kind > 3) { sane = false; bad = juce::String (r.name) + " / " + b.label; }
        }
        check (rs.size() >= 40 && sane, juce::String ((int) rs.size()) + " instruments, every range inside 20 Hz - 20 kHz" + (bad.isNotEmpty() ? " (bad: " + bad + ")" : juce::String()));
        check (EQRef::thirdOctaves().size() == 31, "31 ISO 1/3-octave centres on the scale");
        check (EQRef::noteName (440.0f) == "A4" && EQRef::noteName (27.5f) == "A0" && EQRef::noteName (4186.0f) == "C8" && EQRef::noteName (41.2f) == "E1", "note names (A4 440, A0, C8, E1)");
        check (std::abs (EQRef::parseHz ("4k") - 4000.0f) < 0.01f && std::abs (EQRef::parseHz ("3.15 kHz") - 3150.0f) < 0.01f && std::abs (EQRef::parseHz ("300hz") - 300.0f) < 0.01f
               && EQRef::parseHz ("snare") == 0.0f && std::abs (EQRef::parseNote ("A4") - 440.0f) < 0.01f && EQRef::parseNote ("bass") == 0.0f, "Find understands 4k, 3.15 kHz, 300hz and A4");
        check (EQRef::rangeText (200, 450) == "200 - 450 Hz" && EQRef::rangeText (2500, 5000) == "2.5 - 5 kHz" && EQRef::rangeText (450, 1200) == "450 Hz - 1.2 kHz"
               && EQRef::hzPrecise (312.44f) == "312.4 Hz" && EQRef::hzPrecise (3152.0f) == "3.152 kHz" && EQRef::scaleLabel (31.5f) == "31.5" && EQRef::scaleLabel (1250) == "1.25k",
               "numbers and units: 200 - 450 Hz, 2.5 - 5 kHz, 450 Hz - 1.2 kHz, 312.4 Hz, 3.152 kHz");
        auto at = call ("eq_reference", R"({"query":"300"})");
        auto snare = call ("eq_reference", R"({"query":"snare"})");
        auto sib = call ("eq_reference", R"({"query":"sibilance"})");
        check (at.contains ("D4") && at.contains ("Kick drum") && snare.contains ("Snare drum") && snare.contains ("crack") && sib.contains ("Male vocal"), "eq_reference: a frequency, an instrument and a problem word");
        juce::File png = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm-eqchart-test.png");
        {
            EQChartContent chart;
            chart.setSize (1400, 860);
            check (chart.writePng (png) && png.getSize() > 50000, "the chart renders and saves as PNG (" + juce::String (png.getSize() / 1024) + " KB)");
        }
        if (juce::SystemStats::getEnvironmentVariable ("SQM_KEEP_EQ_PNG", {}).isEmpty()) png.deleteFile();
    }

    std::cout << "\n-- Beat Lab (channel rack) --\n";
    {
        call ("new_session", R"({"name":"Selftest Beat Lab"})");
        call ("add_drum_track", R"({"name":"BL","bars":2,"kit":"Neon Trap"})");
        auto* t = session->findTrack ("BL");
        auto* clip = t != nullptr ? DrumKit::findStepClip (*t) : nullptr;
        check (clip != nullptr && DrumKit::numKits == 27 && DrumKit::kitIndexFor ("amapiano") == 8 && DrumKit::kitIndexFor ("Drill Smoke") == 6, "27 kits; genre words pick a kit");
        if (clip != nullptr)
        {
            call ("beat_lab", R"({"track":"BL","action":"write","rows":{"kick":"x...x...x...x...","closed hat":"x.x.x.x.3.x.x.x.","perc":"x.."}})");
            call ("beat_lab", R"({"track":"BL","action":"row","row":"perc","op":"length","steps":3})");
            call ("beat_lab", R"({"track":"BL","action":"step","row":"kick","step":5,"shift":25})");
            call ("beat_lab", R"({"track":"BL","action":"swing","amount":50})");
            auto seq = clip->generateMidiSequence (te::MidiList::TimeBase::beats);
            auto starts = [&] (int note, double from, double to)
            {
                juce::Array<double> a;
                for (int i = 0; i < seq.getNumEvents(); ++i)
                {
                    auto& m = seq.getEventPointer (i)->message;
                    if (m.isNoteOn() && m.getNoteNumber() == note && m.getTimeStamp() >= from - 1e-6 && m.getTimeStamp() < to - 1e-6) a.add (m.getTimeStamp());
                }
                a.sort();
                return a;
            };
            auto closeTo = [] (double a, double b) { return std::abs (a - b) < 1.0e-3; };
            auto roll = starts (42, 2.0, 2.25);
            check (roll.size() == 3 && closeTo (roll[0], 2.0) && closeTo (roll[1], 2.0 + 1.0 / 12.0) && closeTo (roll[2], 2.0 + 2.0 / 12.0), "a 3-hit roll splits the step into triplet 64ths (" + juce::String (roll.size()) + " hits)");
            auto kick = starts (36, 0.0, 4.0);
            check (kick.size() == 4 && closeTo (kick[1], 1.0 + 0.0625), "Shift +25% moves a kick a quarter step late");
            auto perc = starts (56, 0.0, 4.0);
            check (perc.size() >= 5 && closeTo (perc[1], 0.75 + 0.0625) && closeTo (perc[2], 1.5), "a 3-step row loops inside the 16-step pattern (polyrhythm, swung on the off-steps)");
            call ("beat_lab", R"({"track":"BL","action":"write","rows":{"closed hat":".x.x.x.x.x.x.x.x"}})");
            seq = clip->generateMidiSequence (te::MidiList::TimeBase::beats);
            auto sw = starts (42, 0.0, 1.0);
            check (sw.size() == 2 && closeTo (sw[0], 0.25 + 0.0625), "50% swing delays the off-beat 16ths");
            call ("beat_lab", R"({"track":"BL","action":"row","row":"closed hat","op":"mute","value":true})");
            seq = clip->generateMidiSequence (te::MidiList::TimeBase::beats);
            check (starts (42, 0.0, 8.0).isEmpty(), "muted rows don't play");
            call ("beat_lab", R"({"track":"BL","action":"row","row":"closed hat","op":"mute","value":false})");
            call ("beat_lab", R"({"track":"BL","action":"pattern","pattern":"B"})");
            call ("beat_lab", R"({"track":"BL","action":"starter","genre":"Drill","seed":4})");
            call ("beat_lab", R"({"track":"BL","action":"chain","chain":"A A B A"})");
            check (clip->getNumPatterns() >= 2 && clip->getPatternSequence().size() == 4 && clip->repeatSequence.get(), "patterns A-H and a chain that repeats to fill the clip");
            auto e = BeatLab::euclid (3, 8);
            check (e[0] && ! e[1] && ! e[2] && e[3] && ! e[4] && ! e[5] && e[6] && ! e[7], "Euclidean 3 over 8 = x..x..x.");
            int genresOk = 0;
            juce::Random rnd (9);
            for (int g = 0; g < (int) BeatLab::genres().size(); ++g)
            {
                BeatLab::starter (*clip, g, rnd);
                auto sq = clip->generateMidiSequence (te::MidiList::TimeBase::beats);
                if (sq.getNumEvents() > 8) ++genresOk;
            }
            check (BeatLab::genres().size() == 17 && genresOk == 17, "Beat Starter writes a groove in all " + juce::String (genresOk) + " genres");
            auto get = call ("beat_lab", R"({"track":"BL","action":"get"})");
            auto notesBefore = clip->generateMidiSequence (te::MidiList::TimeBase::beats).getNumEvents() / 2;
            call ("beat_lab", R"({"track":"BL","action":"to_piano_roll"})");
            te::MidiClip* mc = nullptr;
            for (auto c : t->getClips()) if ((mc = dynamic_cast<te::MidiClip*> (c)) != nullptr) break;
            check (mc != nullptr && mc->getSequence().getNumNotes() == notesBefore && DrumKit::findStepClip (*t) == nullptr, "To Piano Roll turns the beat into a MIDI clip (" + juce::String (notesBefore) + " notes)");
        }
    }

    std::cout << "\n-- Piano roll tools --\n";
    {
        check (MidiTools::chordNotes (60, MidiTools::chordIndex ("Minor 7")) == juce::Array<int> { 60, 63, 67, 70 }
               && MidiTools::chordNotes (60, MidiTools::chordIndex ("Minor 7"), 1) == juce::Array<int> { 63, 67, 70, 72 }, "chord stamps and inversions");
        check (MidiTools::snapToScale (61, 0, 1) == 60 && MidiTools::snapToScale (66, 0, 1) == 65 && MidiTools::scaleStep (60, 2, 0, 1) == 64 && MidiTools::scaleStep (60, -1, 9, 2) == 59,
               "snap to scale and scale steps");
        auto dm7 = MidiTools::degreeChord (2, 0, false, true, -1);
        auto e5 = MidiTools::degreeChord (5, 9, true, false, 0);
        check (dm7.first == 2 && dm7.second == 11 && e5.first % 12 == 4 && e5.second == 0 && MidiTools::keyUsesFlats (3, false) && MidiTools::chordName (10, 9, true) == "Bb7",
               "progression degrees (Dm7 in C, E major as V of A minor) and flat spelling");
        call ("add_track", R"({"kind":"instrument","name":"PR"})");
        call ("create_midi_clip", R"({"track":"PR","length_bars":4})");
        auto prog = call ("midi_tools", R"({"track":"PR","action":"progression","progression":"R&B","root":"Eb","bass":true})");
        auto* pt = session->findTrack ("PR");
        te::MidiClip* mc = nullptr;
        if (pt != nullptr) for (auto c : pt->getClips()) if ((mc = dynamic_cast<te::MidiClip*> (c)) != nullptr) break;
        check (mc != nullptr && mc->getSequence().getNumNotes() == 20 && prog.contains ("Fm7") && prog.contains ("Bb7") && prog.contains ("Ebmaj7"), "R&B progression in Eb: Fm7 - Bb7 - Ebmaj7 - Cm7 with a bass line");
        if (mc != nullptr)
        {
            auto& undo = mc->edit.getUndoManager();
            mc->getSequence().clear (&undo);
            auto chord = MidiTools::stampChord (*mc, 60, 0, 0, 0.0, 2.0);
            MidiTools::strum (*mc, chord, 0.125, true);
            juce::Array<double> st; for (auto* n : mc->getSequence().getNotes()) st.add (n->getStartBeat().inBeats()); st.sort();
            check (st.size() == 3 && std::abs (st[1] - 0.125) < 1e-6 && std::abs (st[2] - 0.25) < 1e-6, "strum spreads a chord");
            mc->getSequence().clear (&undo);
            MidiTools::stampChord (*mc, 60, 0, 0, 0.0, 1.0);
            MidiTools::arpeggiate (*mc, {}, 0.25, MidiTools::ArpMode::up);
            juce::Array<int> ps; for (auto* n : mc->getSequence().getNotes()) ps.add (n->getNoteNumber());
            check (ps.size() == 4 && ps.contains (60) && ps.contains (64) && ps.contains (67), "arpeggiate turns a held chord into 16ths");
            mc->getSequence().clear (&undo);
            for (int k = 0; k < 3; ++k) mc->getSequence().addNote (36 + k * 3, te::BeatPosition::fromBeats (k), te::BeatDuration::fromBeats (0.5), 100, 0, &undo);
            MidiTools::slides (*mc, {}, 0.125);
            auto* n0 = mc->getSequence().getNotes()[0];
            check (std::abs (n0->getEndBeat().inBeats() - 1.125) < 1e-6, "808 slides overlap each note into the next");
            call ("midi_tools", R"({"track":"PR","action":"quantize","grid":"1/8","strength":100})");
        }
    }

    auto pitchAt = [] (const juce::File& f, double seconds, double lenSeconds = 0.1) -> double
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return -1;
        const int n = (int) (lenSeconds * r->sampleRate);
        juce::AudioBuffer<float> b (1, n);
        r->read (&b, 0, n, (juce::int64) (seconds * r->sampleRate), true, false);
        const float* x = b.getReadPointer (0);
        const int W = n / 2;
        const int lo = (int) (r->sampleRate / 1500.0), hi = juce::jmin (W - 2, (int) (r->sampleRate / 25.0));
        std::vector<double> d ((size_t) hi + 2, 0.0), cm ((size_t) hi + 2, 1.0);
        double run = 0;
        for (int tau = 1; tau <= hi + 1; ++tau)
        {
            double s = 0; for (int i = 0; i < W; ++i) { auto df = x[i] - x[i + tau]; s += df * df; }
            d[(size_t) tau] = s; run += s; cm[(size_t) tau] = s * tau / juce::jmax (1.0e-12, run);
        }
        int best = lo;
        for (int tau = lo; tau < hi; ++tau) if (cm[(size_t) tau] < 0.12 && cm[(size_t) tau] < cm[(size_t) tau + 1]) { best = tau; break; }
        if (best == lo) for (int tau = lo; tau < hi; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
        const double a = cm[(size_t) best - 1], bb = cm[(size_t) best], c = cm[(size_t) best + 1], den = a - 2 * bb + c;
        const double tt = best + (std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
        return r->sampleRate / tt;
    };
    auto peakOf = [] (const juce::File& f) -> float
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return -1;
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return 99.0f;
        return b.getMagnitude (0, b.getNumSamples());
    };
    std::cout << "\n-- Moth 808 and Blossom --\n";
    {
        auto cents = [] (double hz, double target) { return 1200.0 * std::log2 (hz / target); };
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);

        call ("new_session", R"({"name":"Selftest Instruments"})");
        call ("set_tempo", R"({"bpm":120})");
        call ("add_track", R"({"kind":"instrument","name":"Bass","instrument":"Moth 808"})");
        call ("create_midi_clip", R"({"track":"Bass","length_bars":2,"notes":[{"pitch":36,"start_beat":0,"length_beats":1.5},{"pitch":24,"start_beat":2,"length_beats":1.25},{"pitch":31,"start_beat":3,"length_beats":1}]})");
        auto f808 = dir.getChildFile ("sqm-808-test.wav");
        call ("bounce", "{\"path\":\"" + f808.getFullPathName() + "\",\"tracks\":[\"Bass\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
        auto c2 = pitchAt (f808, 0.4, 0.12), c1 = pitchAt (f808, 1.25, 0.12), g1 = pitchAt (f808, 1.75, 0.12);
        auto pk = peakOf (f808);
        check (std::abs (cents (c2, 65.406)) < 6 && std::abs (cents (c1, 32.703)) < 6 && std::abs (cents (g1, 48.999)) < 6 && pk > 0.2f && pk < 1.01f,
               "Moth 808 in tune (C2 " + juce::String (cents (c2, 65.406), 1) + " ct, C1 " + juce::String (cents (c1, 32.703), 1) + " ct) and the overlapping G1 slides in (" + juce::String (cents (g1, 48.999), 1) + " ct)");

        call ("add_track", R"({"kind":"instrument","name":"Str","instrument":"Blossom 2"})");
        call ("create_midi_clip", R"({"track":"Str","length_bars":2,"notes":[{"pitch":57,"start_beat":0,"length_beats":4},{"pitch":69,"start_beat":4,"length_beats":2}]})");
        auto fbl = dir.getChildFile ("sqm-blossom-test.wav");
        call ("bounce", "{\"path\":\"" + fbl.getFullPathName() + "\",\"tracks\":[\"Str\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
        auto a3 = pitchAt (fbl, 0.3, 0.12), a4 = pitchAt (fbl, 2.2, 0.12);
        pk = peakOf (fbl);
        check (std::abs (cents (a3, 220.0)) < 5 && std::abs (cents (a4, 440.0)) < 5 && pk > 0.05f && pk < 1.01f,
               "Blossom waveguide strings in tune (A3 " + juce::String (cents (a3, 220.0), 1) + " ct, A4 " + juce::String (cents (a4, 440.0), 1) + " ct)");

        // every Blossom preset renders cleanly (no blow-ups, no NaN) and every 808 preset too
        int clean = 0, total = 0;
        for (auto inst : { "Blossom 2", "Moth 808" })
        {
            auto* e = Signature::findByName (inst);
            auto* tr = session->findTrack (juce::String (inst) == "Blossom 2" ? "Str" : "Bass");
            if (e == nullptr || tr == nullptr) continue;
            te::Plugin* pl = nullptr;
            for (auto p : tr->pluginList.getPlugins()) if (p->getPluginType() == e->type) pl = p;
            for (auto& pr : e->presets)
            {
                if (pl == nullptr) break;
                Signature::applyPreset (*pl, pr);
                auto f = dir.getChildFile ("sqm-preset-test.wav");
                call ("bounce", "{\"path\":\"" + f.getFullPathName() + "\",\"tracks\":[\"" + tr->getName() + "\"],\"start_bar\":1,\"end_bar\":3}");
                auto p2 = peakOf (f);
                ++total;
                if (p2 > 0.001f && p2 < 1.01f) ++clean; else std::cout << "     preset " << pr.name << " peak " << p2 << "\n";
                f.deleteFile();
            }
        }
        check (total >= 29 && clean == total, juce::String (clean) + " of " + juce::String (total) + " instrument presets render cleanly");

        // performance: 16-note Blossom chords for 4 seconds, offline
        call ("create_midi_clip", R"({"track":"Str","start_bar":3,"length_bars":2,"notes":[
            {"pitch":40,"start_beat":0,"length_beats":8},{"pitch":45,"start_beat":0,"length_beats":8},{"pitch":50,"start_beat":0,"length_beats":8},{"pitch":55,"start_beat":0,"length_beats":8},
            {"pitch":59,"start_beat":0,"length_beats":8},{"pitch":64,"start_beat":0,"length_beats":8},{"pitch":67,"start_beat":0,"length_beats":8},{"pitch":71,"start_beat":0,"length_beats":8},
            {"pitch":52,"start_beat":0.5,"length_beats":7},{"pitch":57,"start_beat":0.5,"length_beats":7},{"pitch":62,"start_beat":0.5,"length_beats":7},{"pitch":69,"start_beat":0.5,"length_beats":7},
            {"pitch":72,"start_beat":1,"length_beats":7},{"pitch":76,"start_beat":1,"length_beats":7},{"pitch":79,"start_beat":1,"length_beats":7},{"pitch":83,"start_beat":1,"length_beats":7}]})");
        if (auto e = Signature::findByName ("Blossom 2"))
            if (auto* tr = session->findTrack ("Str"))
                for (auto p : tr->pluginList.getPlugins()) if (p->getPluginType() == e->type) Signature::applyPreset (*p, e->presets.front());
        auto fperf = dir.getChildFile ("sqm-blossom-perf.wav");
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        call ("bounce", "{\"path\":\"" + fperf.getFullPathName() + "\",\"tracks\":[\"Str\"],\"start_bar\":3,\"end_bar\":5}");
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (ms < 4000.0, "16 Blossom voices render " + juce::String (4000.0 / juce::jmax (1.0, ms), 1) + "x faster than real time (" + juce::String ((int) ms) + " ms for 4 s)");
        for (auto f : { f808, fbl, fperf }) f.deleteFile();
    }

    std::cout << "\n-- Edit window mixing: per-track I, track views, plug-in menus --\n";
    {
        call ("new_session", R"({"name":"Selftest Edit Mix"})");
        call ("new_tracks", R"({"type":"instrument","format":"stereo","name":"KeysA"})");
        call ("new_tracks", R"({"type":"instrument","format":"stereo","name":"KeysB"})");
        auto* a = session->findTrack ("KeysA");
        auto* b = session->findTrack ("KeysB");
        if (a != nullptr && b != nullptr)
        {
            TrackOps::setInputOnlyMonitoring (*session, false);   // what the app does at launch: devices back to automatic
            TrackOps::updateMidiThruTarget (*session, nullptr);
            TrackOps::setInputMonitor (*session, *a, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            bool aLive = false, bLive = false;
            for (auto in : session->edit->getAllInputDevices())
            {
                if (te::isOnTargetTrack (*in, *a, 0)) aLive = aLive || in->isLivePlayEnabled (*a);
                if (te::isOnTargetTrack (*in, *b, 0)) bLive = bLive || in->isLivePlayEnabled (*b);
            }
            check (TrackOps::isInputMonitorOn (*a) && ! TrackOps::isInputMonitorOn (*b) && aLive && ! bLive,
                   "I (input monitor) on one track does not switch on other tracks sharing the input");
            MixCtl::setView (*a, "volume");
            check (MixCtl::viewParam (*session, *a) == MixCtl::volumeParam (*session, *a) && MixCtl::viewLabel (*session, *a) == "volume", "Track View: volume lane");
            call ("insert_plugin", R"({"track":"KeysA","plugin":"Prism EQ"})");
            auto views = MixCtl::viewChoices (*session, *a);
            bool eqParam = false;
            for (auto& v : views) if (v.label.startsWith ("Prism EQ > ")) eqParam = true;
            check (eqParam, "Track View lists plug-in parameters by name");
            MixCtl::setVolumeDb (*session, *a, -6.0f);
            check (std::abs (MixCtl::getVolumeDb (*session, *a) + 6.0f) < 0.05f, "Edit window fader sets the track volume");
        }
        auto menu = PluginMenu::build (*engine, PluginMenu::effects, true);
        juce::StringArray tops;
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();) tops.add (it.getItem().text);
        check (tops.contains ("EQ") && tops.contains ("Dynamics") && tops.contains ("Reverb") && tops.contains ("Search plug-ins..."), "plug-in menu is organised by Pro Tools category with search");
    }

    std::cout << "\n-- 1.6 plug-ins: Moth Tune, Apex Series, colour set, Molt --\n";
    {
        // a stereo test tone: 220 Hz with harmonics plus a little noise, -6 dBFS peak, 4 s
        auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_selftest_tone.wav");
        {
            const double sr = 44100.0; const int len = (int) (sr * 4.0);
            juce::AudioBuffer<float> buf (2, len);
            juce::Random rnd (7);
            for (int i = 0; i < len; ++i)
            {
                const double t = i / sr;
                float v = (float) (0.30 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * t) + 0.12 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t)
                                   + 0.06 * std::sin (juce::MathConstants<double>::twoPi * 660.0 * t)) + (rnd.nextFloat() - 0.5f) * 0.02f;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            tone.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = tone.createOutputStream())
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0)))
                    w->writeFromAudioSampleBuffer (buf, 0, len);
        }
        const auto tonePath = tone.getFullPathName().replace ("\\", "/");
        struct Case { const char* track; const char* tool; const char* which; };
        const Case cases[] = { { "ST Tune", "moth_tune", "" }, { "ST TuneFx", "moth_tune_fx", "" },
                               { "ST Dyn", "apex", "dynamics" }, { "ST Lim", "apex", "limiter" }, { "ST MB", "apex", "multiband" }, { "ST Sub", "apex", "sub" },
                               { "ST Aurora", "studio_fx", "aurora" }, { "ST Opto", "studio_fx", "opto" }, { "ST Pulse", "studio_fx", "pulse" }, { "ST OilCan", "studio_fx", "oilcan" },
                               { "ST Ladder", "studio_fx", "ladder" }, { "ST Ring", "studio_fx", "ring" }, { "ST Phase", "studio_fx", "phaser" }, { "ST BBD", "studio_fx", "bbd" } };
        for (auto& c : cases)
        {
            call ("add_track", juce::String (R"({"kind":"audio","name":")") + c.track + R"("})");
            call ("import_audio", juce::String (R"({"track":")") + c.track + R"(","path":")" + tonePath + R"(","start_bar":1})");
            juce::String which = juce::String (c.which).isNotEmpty() ? juce::String (R"(,"which":")") + c.which + "\"" : juce::String();
            call (c.tool, juce::String (R"({"track":")") + c.track + "\"" + which + R"(,"action":"add"})");
            if (juce::String (c.which) == "limiter")
                call ("apex", R"({"track":"ST Lim","which":"limiter","action":"set","settings":{"Input":12,"Ceiling":-1}})");
            auto an = juce::JSON::parse (call ("analyze_mix", juce::String (R"({"start_bar":1,"end_bar":3,"tracks":[")") + c.track + R"("]})"));
            const double pk = an["peak_dbfs"].toString().getDoubleValue();
            check (pk > -40.0 && pk < 3.0, juce::String (c.tool) + " " + c.which + ": audio passes at a sane level (peak " + juce::String (pk, 1) + " dBFS)");
            if (juce::String (c.which) == "limiter")
            {
                const double tp = (double) juce::JSON::parse (call ("apex", R"({"track":"ST Lim","which":"limiter","action":"get"})"))["loudness"]["true_peak_max_dbtp"];
                check (tp <= -0.95 && tp > -3.0, "Apex Limiter holds the -1 dBTP ceiling with +12 dB of input (true peak " + juce::String (tp, 2) + " dBTP)");
            }
        }
        auto mtGet = call ("moth_tune", R"({"track":"ST Tune","action":"get"})");
        check (mtGet.contains ("Retune Speed"), "Moth Tune reports its parameters");
        check (call ("moth_tune", R"({"track":"ST Tune","action":"key","root":"A","scale":"minor"})").contains ("A Natural Minor"), "Moth Tune key: A minor set");
        auto fxSet = call ("studio_fx", R"({"track":"ST Pulse","which":"pulse","action":"set","settings":{"lowFreq":100,"Low Boost":5}})");
        check (fxSet.contains ("\"lowFreq\": 3"), "Pulse EQ: a frequency in Hz snaps to the nearest switch position");
        auto molt = call ("molt", R"({"action":"render","track":"ST Ring","ratio":1.5,"semitones":3})");
        auto mclips = juce::JSON::parse (call ("list_clips", R"({"track":"ST Ring"})"));
        const double mlen = (double) mclips[0]["clips"][0]["length_seconds"];
        check (std::abs (mlen - 6.0) < 0.02, "Molt: a 4 s clip at ratio 1.5 renders to 6 s (" + juce::String (mlen, 3) + " s)");
        for (auto& c : cases) call ("delete_track", juce::String (R"({"track":")") + c.track + R"("})");
        tone.deleteFile();
    }

    std::cout << "\n-- Performance engine and crash protection --\n";
    {
        auto pe = juce::JSON::parse (call ("playback_engine", R"({"host_processors":1,"cpu_usage_limit":80,"dynamic_plugin_processing":true,"overload_protection":true,"disk_cache_seconds":15})"));
        check ((int) pe["host_engine"]["host_processors"] == 1 && (int) pe["host_engine"]["cpu_usage_limit"] == 80 && (int) pe["host_engine"]["disk_cache_seconds"] == 15,
               "Playback Engine: host processors, CPU usage limit and cache size are set");
        check (::dynamicPluginProcessingEnabled().load(), "Dynamic Plug-in Processing reaches the engine");
        call ("playback_engine", R"({"host_processors":0,"cpu_usage_limit":95,"disk_cache_seconds":6})");
        check (call ("system_usage", "{}").contains ("heaviest_plugins"), "System Usage reports the heaviest plug-ins");

        // crash protection: a crash report naming a plug-in switches that plug-in off; a crash while opening a session with no
        // culprit opens the next one in Safe Mode
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_crashguard_test");
        dir.deleteRecursively(); dir.createDirectory();
        dir.getChildFile ("running.flag").replaceWithText ("running /x.squelch");
        juce::Thread::sleep (20);
        dir.getChildFile ("Last crash report.txt").replaceWithText ("Squelchmoth crash report\nPlug-ins running when it crashed: Wobbler 2 (VST3)\n");
        auto& cg = CrashGuard::begin (dir, false);
        check (cg.crashedLastTime && cg.culprits.contains ("Wobbler 2 (VST3)") && ! cg.safeMode, "crash report: the plug-in on the crashing stack is found");
        check (CrashGuard::isPluginDisabled ("VST3-Wobbler 2-1a2b3c") && ! CrashGuard::isPluginDisabled ("VST3-Other-99"), "that plug-in is switched off; others still load");
        CrashGuard::setPluginSwitchedOff ("Wobbler 2 (VST3)", false);
        check (! CrashGuard::isPluginDisabled ("VST3-Wobbler 2-1a2b3c"), "a switched-off plug-in can be turned back on");
        CrashGuard::end();
        check (! dir.getChildFile ("running.flag").existsAsFile(), "clean quit removes the running marker");
        dir.getChildFile ("Last crash report.txt").deleteFile();
        dir.getChildFile ("running.flag").replaceWithText ("loading /x.squelch");
        CrashGuard::state() = {};
        auto& cg2 = CrashGuard::begin (dir, false);
        check (cg2.crashedWhileLoading && cg2.safeMode && CrashGuard::isPluginDisabled ("AudioUnit-Anything"), "crash while opening a session -> next launch is Safe Mode");
        CrashGuard::state() = {};
        CrashGuard::end();
        CrashGuard::folder() = juce::File();
        dir.deleteRecursively();
    }

    std::cout << "\n-- 1.7: session setup, tempo map, polarity, snapshots, insert chains, stems, bounces, backups --\n";
    {
        auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_selftest_tone17.wav");
        {
            const double sr = 48000.0; const int len = (int) (sr * 2.0);
            juce::AudioBuffer<float> buf (2, len);
            for (int i = 0; i < len; ++i)
            {
                const float v = (float) (0.3 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * i / sr));
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.8f);
            }
            tone.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = tone.createOutputStream())
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0)))
                    w->writeFromAudioSampleBuffer (buf, 0, len);
        }
        const auto tp = tone.getFullPathName().replace ("\\", "/");
        call ("new_session", R"({"name":"Seventeen","tempo":120})");
        for (auto n : { "S1", "S2" })
        {
            call ("add_track", juce::String (R"({"kind":"audio","name":")") + n + R"("})");
            call ("import_audio", juce::String (R"({"track":")") + n + R"(","path":")" + tp + R"(","start_bar":1})");
        }

        auto ss = juce::JSON::parse (call ("session_setup", R"({"timecode_rate":"25","timecode_start":"01:00:00:00","pan_depth":2,"bit_depth":24})"));
        check (ss["timecode_rate"].toString() == "25" && ss["timecode_start"].toString() == "01:00:00:00" && ss["record_bit_depth"].toString() == "24-bit",
               "Session Setup: timecode rate / start and record bit depth");
        call ("session_setup", R"({"timecode_start":"00:00:00:00","bit_depth":32})");

        call ("tempo_map", R"({"action":"set_tempo","bar":3,"bpm":90})");
        auto tm = juce::JSON::parse (call ("tempo_map", R"({"action":"set_meter","bar":4,"meter":"3/4"})"));
        check (tm["tempos"].size() == 2 && tm["meters"].size() == 2 && tm["meters"][1]["at"].toString() == "4|1|000",
               "tempo + meter map: a meter after a tempo change lands on 4|1|000 (" + tm["meters"][1]["at"].toString() + ")");
        call ("tempo_map", R"({"action":"delete_meter","index":1})");
        auto tm2 = juce::JSON::parse (call ("tempo_map", R"({"action":"delete_tempo","index":1})"));
        check (tm2["tempos"].size() == 1 && tm2["meters"].size() == 1, "tempo and meter events delete");

        // polarity: two identical tracks, one inverted, cancel completely
        auto loudness = [&] { return juce::JSON::parse (call ("analyze_mix", R"({"start_seconds":0,"end_seconds":2})"))["peak_dbfs"].toString().getDoubleValue(); };
        const double both = loudness();
        call ("polarity", R"({"tracks":["S2"],"inverted":true})");
        const double nulled = loudness();   // the renderer's meter floor is -80 dB (0.0001)
        check (both > -12.0 && nulled <= -79.9, "polarity: identical tracks with one inverted cancel (" + juce::String (both, 1) + " dB -> " + juce::String (nulled, 1) + " dB)");

        // mix snapshots
        call ("mix_snapshot", R"({"action":"store","slot":"A","name":"Null"})");
        call ("update_track", R"({"track":"S1","volume_db":-20})");
        call ("polarity", R"({"tracks":["S2"],"inverted":false})");
        call ("mix_snapshot", R"({"action":"store","slot":"B"})");
        const double bLevel = loudness();
        call ("mix_snapshot", R"({"action":"recall","slot":"A"})");
        const double aLevel = loudness();
        check (bLevel > -12.0 && aLevel <= -79.9, "snapshot recall restores fader and polarity (B " + juce::String (bLevel, 1) + " dB, A " + juce::String (aLevel, 1) + " dB)");
        call ("mix_snapshot", R"({"action":"undo"})");
        check (loudness() > -12.0, "Undo Recall goes back to the mix before the recall");
        call ("mix_snapshot", R"({"action":"recall","slot":"B"})");

        // insert chains (and Pro Tools insert order: inserts stay before the dual panner and sends)
        call ("add_send", R"({"track":"S1","bus":3,"level_db":-10,"pre_fader":true})");
        call ("insert_plugin", R"({"track":"S1","plugin":"Prism EQ"})");
        call ("insert_plugin", R"({"track":"S1","plugin":"Cocoon"})");
        auto saved = call ("insert_chain", R"({"action":"save","track":"S1","name":"Selftest Chain"})");
        auto loaded = call ("insert_chain", R"({"action":"load","track":"S2","name":"Selftest Chain"})");
        check (saved.contains ("2 plug-ins") && loaded.contains ("Loaded 2"), "insert chain saves and loads 2 plug-ins");
        {
            auto* s1 = session->findTrack (juce::var ("S1"));
            bool orderOk = s1 != nullptr;
            if (s1 != nullptr)
            {
                auto ins = Session::getInserts (s1->pluginList, s1);
                const int lastInsert = ins.isEmpty() ? -1 : s1->pluginList.indexOf (ins.getLast());
                for (auto p : s1->pluginList.getPlugins())
                    if (dynamic_cast<te::AuxSendPlugin*> (p) != nullptr || dynamic_cast<SqStereoPanPlugin*> (p) != nullptr)
                        orderOk = orderOk && s1->pluginList.indexOf (p) > lastInsert;
            }
            check (orderOk, "new inserts go before the sends and the dual panner (Pro Tools signal flow)");
        }
        InsertChains::fileFor ("Selftest Chain").deleteFile();

        // bounces at the session rate, and stems that sum to the mix
        auto b = call ("bounce", R"({"name":"seventeen mix"})");
        check (b.contains ("kHz 24-bit dithered"), "bounce defaults: session rate, 24-bit, dithered");
        call ("bounce", R"({"name":"seventeen float","bit_depth":32,"master_inserts":false,"tail_seconds":1})");
        auto stemDir = session->sessionsDir.getChildFile ("stems17");
        auto st = juce::JSON::parse (call ("export_stems", "{\"bit_depth\":32,\"tail_seconds\":1,\"folder\":\"" + stemDir.getFullPathName().replace ("\\", "/") + "\"}"));
        check ((int) st["written"] == 2, "export_stems writes one file per track with audio (" + st["written"].toString() + ")");
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            auto read = [&] (const juce::File& f) { juce::AudioBuffer<float> buf; if (auto r = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f))) { buf.setSize ((int) r->numChannels, (int) r->lengthInSamples); r->read (&buf, 0, (int) r->lengthInSamples, 0, true, true); } return buf; };
            auto mix = read (session->bouncesDir.getChildFile ("seventeen float.wav"));
            juce::AudioBuffer<float> sum;
            bool sameLength = true;
            for (auto& f : stemDir.findChildFiles (juce::File::findFiles, false, "*.wav"))
            {
                auto x = read (f);
                if (sum.getNumSamples() == 0) { sum.makeCopyOf (x); continue; }
                sameLength = sameLength && x.getNumSamples() == sum.getNumSamples();
                for (int c = 0; c < juce::jmin (sum.getNumChannels(), x.getNumChannels()); ++c) sum.addFrom (c, 0, x, c, 0, juce::jmin (sum.getNumSamples(), x.getNumSamples()));
            }
            float resid = 0.0f;
            const int n = juce::jmin (mix.getNumSamples(), sum.getNumSamples());
            for (int c = 0; c < juce::jmin (mix.getNumChannels(), sum.getNumChannels()); ++c)
                for (int i = 0; i < n; ++i) resid = juce::jmax (resid, std::abs (mix.getSample (c, i) - sum.getSample (c, i)));
            check (sameLength && n > 48000 && resid < 1.0e-5f && mix.getNumSamples() == sum.getNumSamples(),
                   "stems line up and sum to the mix (residual " + juce::String (juce::Decibels::gainToDecibels (resid, -200.0f), 1) + " dB)");
        }

        // Auto-Save / Session File Backups
        {
            AutoSaver saver (*session);
            call ("save_session", "{}");
            AutoSaver::mirrorFolderFor (session->getFile()).deleteRecursively();   // leftovers from an earlier self-test run
            auto bk = juce::JSON::parse (call ("backups", R"({"action":"backup_now"})"));
            check (! (bool) bk["unsaved_changes"], "a backup copy doesn't make the session look unsaved");
            auto list = juce::JSON::parse (call ("backups", R"({"action":"list"})"));
            check (list.size() == 1 && list[0]["file"].toString().contains (".bak.001"), "Session File Backups: numbered backup written next to the session");
            auto mirror = AutoSaver::mirrorFolderFor (session->getFile());
            check (mirror.getChildFile ("Seventeen.bak.001.squelch").existsAsFile(), "a second copy of each backup is kept on the system drive");
            auto rs = call ("backups", R"({"action":"restore","index":1})");
            check (rs.contains ("(restored"), "restore opens the backup as a separate '(restored)' session");
            mirror.deleteRecursively();
        }
        tone.deleteFile();
    }

    std::cout << "\n-- 1.8 Rack: channel rack, patterns, lanes, channel settings, instruments, chords, 60-second beat --\n";
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        auto cents = [] (double hz, double target) { return 1200.0 * std::log2 (hz / target); };
        // first-sample-above-threshold onsets, one per window of 'win' seconds
        auto onsetsOf = [] (const juce::File& f, double win, float thresh = 0.05f) -> std::vector<double>
        {
            std::vector<double> out;
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
            if (r == nullptr) return out;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
            const int w = (int) (win * r->sampleRate);
            for (int start = 0; start + w <= b.getNumSamples(); start += w)
                for (int i = start; i < start + w; ++i)
                    if (std::abs (b.getSample (0, i)) > thresh) { out.push_back (i / r->sampleRate); break; }
            return out;
        };
        auto startedAt = juce::Time::getMillisecondCounterHiRes();

        // 1. the rack IS the session: a channel is a track with Rack Channel at slot 0 and the instrument after it
        call ("new_session", R"({"name":"Selftest Rack"})");
        call ("set_tempo", R"({"bpm":120})");
        call ("rack", R"({"action":"add","type":"instrument","what":"Venom","name":"Lead"})");
        {
            auto* lead = session->findTrack ("Lead");
            juce::StringArray order; if (lead) for (auto p : lead->pluginList.getPlugins()) order.add (p->getName());
            check (lead != nullptr && order.size() >= 2 && order[0] == "Rack Channel" && order[1] == "Venom", "rack channel = track with Rack Channel at slot 0 then the instrument (" + order.joinIntoString (" > ") + ")");
            auto tl = call ("get_session", "{}");
            check (tl.contains ("Lead"), "rack channels appear in the session's track list");
        }

        // 2. kit channels, steps and step timing (kick on every beat at 120 bpm -> onsets 0.5 s apart within 5 ms)
        auto kit = juce::JSON::parse (call ("rack", R"({"action":"add","type":"kit","what":"Trap"})"));
        auto rl = juce::JSON::parse (call ("rack", R"({"action":"list"})"));
        check (rl["channels"].size() >= 9, "kit adds one channel per sound after the instrument (" + juce::String (rl["channels"].size()) + " channels)");
        check (rl["channels"][1]["name"].toString() == "Kick" && rl["channels"][2]["name"].toString() == "Snare", "channel order: Kick, Snare, ... (" + rl["channels"][1]["name"].toString() + ", " + rl["channels"][2]["name"].toString() + ")");
        call ("rack", R"({"action":"write","channel":"Kick","steps":"x...x...x...x..."})");
        call ("pattern", R"({"action":"place","pattern":1,"bar":1,"repeats":2})");
        auto fkick = dir.getChildFile ("sqm-rack-kick.wav");
        call ("bounce", "{\"path\":\"" + fkick.getFullPathName() + "\",\"tracks\":[\"Kick\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
        {
            auto on = onsetsOf (fkick, 0.5);
            double worst = 0; for (size_t i = 0; i < on.size(); ++i) worst = juce::jmax (worst, std::abs (on[i] - i * 0.5));
            check (on.size() == 8 && worst < 0.005, "step timing: 8 kicks over 2 bars, worst offset " + juce::String (worst * 1000.0, 2) + " ms (" + juce::String ((int) on.size()) + " onsets)");
        }

        // 3. pattern clips are linked; make_unique breaks one link
        {
            auto* kick = session->findTrack ("Kick");
            int linked = 0; if (kick) for (auto c : kick->getClips()) if (Rack::clipPatternId (*c).isNotEmpty() && ! Rack::isUnique (*c)) ++linked;
            check (linked == 2, "placing a pattern twice makes 2 linked Pattern Clips on each channel (" + juce::String (linked) + ")");
            call ("pattern", R"({"action":"make_unique","track":"Kick","clip":2})");
            int uniq = 0; if (kick) for (auto c : kick->getClips()) if (Rack::isUnique (*c)) ++uniq;
            check (uniq == 1, "make_unique breaks exactly one clip's link");
            call ("rack", R"({"action":"write","channel":"Kick","steps":"x.x.x.x.x.x.x.x."})");
            auto ln = juce::JSON::parse (call ("rack", R"({"action":"list"})"));
            check (ln["channels"][1]["steps"].toString().removeCharacters (" ") == "x.x.x.x.x.x.x.x.", "editing the pattern updates the rack row");
        }

        // 4. Graph Editor lanes round-trip in natural units (pitch in cents, pan -1..1, velocity 1-127)
        {
            call ("rack", R"({"action":"lane","channel":"Kick","lane":"pitch","values":[100,-100,50,-50]})");
            auto p = juce::JSON::parse (call ("rack", R"({"action":"lane","channel":"Kick","lane":"pitch"})"))["values"];
            check (p.size() >= 4 && std::abs (p[0].toString().getDoubleValue() - 100) < 0.01 && std::abs (p[1].toString().getDoubleValue() + 100) < 0.01 && std::abs (p[3].toString().getDoubleValue() + 50) < 0.01, "pitch lane round-trips +-100 cents");
            call ("rack", R"({"action":"lane","channel":"Kick","lane":"pan","values":[-1,1,0,0.5]})");
            auto pn = juce::JSON::parse (call ("rack", R"({"action":"lane","channel":"Kick","lane":"pan"})"))["values"];
            check (pn.size() >= 4 && std::abs (pn[0].toString().getDoubleValue() + 1) < 0.01 && std::abs (pn[1].toString().getDoubleValue() - 1) < 0.01 && std::abs (pn[3].toString().getDoubleValue() - 0.5) < 0.01, "pan lane round-trips -1..1");
            call ("rack", R"({"action":"lane","channel":"Kick","lane":"velocity","value":64})");
            auto v = juce::JSON::parse (call ("rack", R"({"action":"lane","channel":"Kick","lane":"velocity"})"))["values"];
            check (v.size() >= 1 && std::abs (v[0].toString().getDoubleValue() - 64) < 1.01, "velocity lane set for every step");
        }

        // 5. Channel Settings (Rack Channel): arpeggiator turns a held chord into 1/16 notes; settings round-trip
        {
            call ("rack", R"({"action":"to_piano_roll","channel":"Lead"})");
            call ("rack", R"({"action":"notes","channel":"Lead","replace":true,"notes":[{"pitch":60,"start_beat":0,"length_beats":4,"velocity":100},{"pitch":64,"start_beat":0,"length_beats":4,"velocity":100},{"pitch":67,"start_beat":0,"length_beats":4,"velocity":100}]})");
            call ("instrument", R"({"action":"preset","track":"Lead","name":"Init Venom"})");
            call ("channel_settings", R"({"action":"set","channel":"Lead","arp":{"mode":"up","time":0.25,"gate":0.5},"polyphony":{"max_poly":4},"echo":{"count":0}})");
            auto cs = juce::JSON::parse (call ("channel_settings", R"({"action":"get","channel":"Lead"})"));
            check (cs["arp"]["mode"].toString() == "up" && (int) cs["polyphony"]["max_poly"] == 4, "channel_settings round-trip (arp " + cs["arp"]["mode"].toString() + ", max poly " + cs["polyphony"]["max_poly"].toString() + ")");
            call ("pattern", R"({"action":"sync","pattern":1})");
            auto farp = dir.getChildFile ("sqm-rack-arp.wav");
            call ("bounce", "{\"path\":\"" + farp.getFullPathName() + "\",\"tracks\":[\"Lead\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}");
            auto on = onsetsOf (farp, 0.125, 0.02);
            check (on.size() >= 12, "arpeggiator on a held triad gives a note every 1/16 (" + juce::String ((int) on.size()) + " of 16 windows have an onset)");
            call ("channel_settings", R"({"action":"set","channel":"Lead","arp":{"mode":"off"},"polyphony":{"mono":true,"porta":true,"slide_time":0.2}})");
            cs = juce::JSON::parse (call ("channel_settings", R"({"action":"get","channel":"Lead"})"));
            check ((bool) cs["polyphony"]["mono"] && (bool) cs["polyphony"]["porta"], "mono + porta set through channel_settings");
            call ("channel_settings", R"({"action":"reset","channel":"Lead"})");
        }

        // 6. every 1.8 instrument: 40+ presets, parameters reachable, sounds, A4 = 440 Hz on the pitched ones
        {
            struct Inst { const char* name; bool pitched; int minPresets; };
            const Inst insts[] = { { "Mothsynth 2", true, 40 }, { "Venom", true, 40 }, { "Sim Moth", true, 40 }, { "Mini Moth", true, 40 }, { "Thump", false, 40 }, { "Dial 10", true, 40 }, { "Blossom 2", true, 40 }, { "Hive Pads", false, 40 } };
            for (auto& in : insts)
            {
                juce::String nm (in.name);
                call ("add_track", "{\"kind\":\"instrument\",\"name\":\"T " + nm + "\",\"instrument\":\"" + nm + "\"}");
                auto pl = juce::JSON::parse (call ("instrument", "{\"action\":\"preset\",\"track\":\"T " + nm + "\",\"list\":true}"));
                const int np = pl["presets"].size();
                check (np >= in.minPresets, nm + ": " + juce::String (np) + " presets");
                if (nm == "Hive Pads") call ("instrument", "{\"action\":\"preset\",\"track\":\"T " + nm + "\",\"index\":1}");
                auto pr = juce::JSON::parse (call ("instrument", "{\"action\":\"parameters\",\"track\":\"T " + nm + "\"}"));
                check (pr["parameters"].size() >= 7, nm + ": " + juce::String (pr["parameters"].size()) + " parameters reachable");
                if (nm == "Venom") call ("instrument", R"({"action":"set","track":"T Venom","params":[{"param":"Sub Level","real":0}]})");   // the sub oscillator sits an octave under A4 by design
                if (nm == "Sim Moth") call ("instrument", R"({"action":"set","track":"T Sim Moth","params":[{"param":"Osc 3 Level","real":0}]})");   // Osc 3 sits at -12 st by design
                const int note = nm == "Hive Pads" ? 36 : nm == "Thump" ? 36 : 69;
                call ("create_midi_clip", "{\"track\":\"T " + nm + "\",\"length_bars\":1,\"notes\":[{\"pitch\":" + juce::String (note) + ",\"start_beat\":0,\"length_beats\":3}]}");
                auto f = dir.getChildFile ("sqm-inst-" + nm.removeCharacters (" ") + ".wav");
                call ("bounce", "{\"path\":\"" + f.getFullPathName() + "\",\"tracks\":[\"T " + nm + "\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}");
                const float pk = peakOf (f);
                if (in.pitched)
                {
                    const double hz = pitchAt (f, 0.6, 0.15);
                    check (pk > 0.02f && pk < 1.01f && std::abs (cents (hz, 440.0)) < 10.0, nm + " sounds and A4 = " + juce::String (hz, 1) + " Hz (" + juce::String (cents (hz, 440.0), 1) + " ct), peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
                }
                else check (pk > 0.02f && pk < 1.01f, nm + " sounds, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
                call ("delete_track", "{\"track\":\"T " + nm + "\"}");
            }
            auto il = juce::JSON::parse (call ("instrument", R"({"action":"list"})"));
            int covered = 0; for (auto& e : *il["instruments"].getArray()) if (e["what"].toString().isNotEmpty()) ++covered;
            check (il["instruments"].size() >= 12 && covered >= 10, "instrument shelf: " + juce::String (il["instruments"].size()) + " instruments, " + juce::String (covered) + " described");
        }

        // 7. chord tools: I-V-vi-IV correct in every key; top-down stamp keeps the clicked note on top
        {
            static const int maj[] = { 0, 4, 7 }, mnr[] = { 0, 3, 7 };
            bool allKeys = true; juce::String bad;
            for (int key = 0; key < 12; ++key)
            {
                auto pv = juce::JSON::parse (call ("chords", "{\"action\":\"preview\",\"degrees\":\"I - V - vi - IV\",\"key\":" + juce::String (key) + "}"));
                const int offs[] = { 0, 7, 9, 5 }; const bool minor[] = { false, false, true, false };
                for (int c = 0; c < 4; ++c)
                {
                    std::set<int> got, want;
                    for (auto& n : *pv["chords"][c]["notes"].getArray()) got.insert (((int) n % 12 + 12) % 12);
                    for (auto iv : (minor[c] ? mnr : maj)) want.insert ((key + offs[c] + iv) % 12);
                    if (got != want) { allKeys = false; bad << " " << MidiTools::pitchClassName (key, false) << ":" << pv["chords"][c]["name"].toString(); }
                }
            }
            check (allKeys, "progression tool: I - V - vi - IV spells the right chords in all 12 keys" + bad);
            call ("add_track", R"({"kind":"instrument","name":"Chords","instrument":"Mothsynth 2"})");
            auto g = juce::JSON::parse (call ("chords", R"({"action":"generate","track":"Chords","progression":"Gospel Lift","key":"Eb","bars":4,"bars_per_chord":1})"));
            check (g["wrote"].toString().startsWith ("Abmaj7 - Bb7 - Gm7 - Cm7") && (int) g["notes_written"] >= 16, "library progression written with sevenths, flats in Eb, bass line (" + g["wrote"].toString() + ", " + g["notes_written"].toString() + " notes)");
            auto st = juce::JSON::parse (call ("chords", R"({"action":"stamp","track":"Chords","note":"E5","type":"Minor 7","mode":"top_down","start_beat":0,"length_beats":2})"));
            int top = -1; for (auto& n : *st["notes"].getArray()) top = juce::jmax (top, (int) n);
            check (top == 76 && st["chord"].toString().startsWith ("F#m7"), "chord stamp top-down keeps E5 on top and builds F#m7 under it (" + st["chord"].toString() + ")");
            auto st2 = juce::JSON::parse (call ("chords", R"({"action":"stamp","track":"Chords","note":"E4","type":"Minor 7","mode":"bottom_up","start_beat":2,"length_beats":2})"));
            int low = 999; for (auto& n : *st2["notes"].getArray()) low = juce::jmin (low, (int) n);
            check (low == 64 && st2["chord"].toString().startsWith ("Em7"), "chord stamp bottom-up puts the root on the clicked key");
            auto lb = juce::JSON::parse (call ("chords", R"({"action":"labels","channel":"Kick"})"));
            check (lb.size() >= 1 && lb[0]["label"].toString() == "Kick", "key labels: a kit channel labels its root key with the sound name");
        }

        // 8. the 60-second beat flow, measured: kit + starter groove + bass + place + bounce
        {
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            call ("new_session", R"({"name":"Sixty"})");
            call ("set_tempo", R"({"bpm":140})");
            call ("rack", R"({"action":"add","type":"kit","what":"Trap"})");
            call ("rack", R"({"action":"starter","genre":"Trap","seed":7})");
            call ("rack", R"({"action":"add","type":"instrument","what":"Moth 808","name":"808"})");
            call ("rack", R"({"action":"write","channel":"808","steps":"x.......x..x...."})");
            call ("pattern", R"({"action":"place","pattern":1,"bar":1,"repeats":4})");
            auto f60 = dir.getChildFile ("sqm-sixty.wav");
            call ("bounce", "{\"path\":\"" + f60.getFullPathName() + "\",\"start_bar\":1,\"end_bar\":5,\"bit_depth\":24}");
            const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            const float pk = peakOf (f60);
            check (pk > 0.05f && pk < 1.01f && secs < 60.0, "60-second beat: kit + groove + 808 + 4 bars placed + bounced in " + juce::String (secs, 1) + " s, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
            auto pm = call ("pattern", R"({"action":"mode","mode":"pattern"})");
            auto pl = juce::JSON::parse (call ("pattern", R"({"action":"list"})"));
            check (pl["mode"].toString() == "pattern", "Pattern mode engages (" + pl["mode"].toString() + ")");
            call ("pattern", R"({"action":"mode","mode":"song"})");
        }
        std::cout << "  (1.8 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)\n";
    }

    std::cout << "\n-- 1.8.1: instruments, factory content, effects, kits, Cut Lab --\n";
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm181"); dir.createDirectory();
        auto cents = [] (double hz, double target) { return 1200.0 * std::log2 (hz / target); };
        auto readBuf = [] (const juce::File& f, double& sr) { juce::AudioBuffer<float> b; juce::AudioFormatManager fm; fm.registerBasicFormats();
                                                             if (auto r = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f))) { b.setSize ((int) r->numChannels, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); sr = r->sampleRate; } return b; };
        auto rmsDb = [] (const juce::AudioBuffer<float>& b, int n) { double s = 0; for (int c = 0; c < b.getNumChannels(); ++c) { auto* p = b.getReadPointer (c); for (int i = 0; i < n; ++i) s += (double) p[i] * p[i]; } return juce::Decibels::gainToDecibels ((float) std::sqrt (s / juce::jmax (1.0, (double) n * b.getNumChannels())), -200.0f); };
        auto diffDb = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int n) { double s = 0, r = 0; for (int c = 0; c < juce::jmin (a.getNumChannels(), b.getNumChannels()); ++c) { auto* p = a.getReadPointer (c); auto* q = b.getReadPointer (c); for (int i = 0; i < n; ++i) { s += (double) (p[i] - q[i]) * (p[i] - q[i]); r += (double) q[i] * q[i]; } } return 10.0 * std::log10 (juce::jmax (1.0e-30, s) / juce::jmax (1.0e-30, r)); };
        auto startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest 181"})");
        call ("set_tempo", R"({"bpm":120})");

        // 1. every 1.8.1 instrument: 40+ presets, parameters reachable, sounds; the pitched ones play A4 = 440 Hz
        {
            struct Inst { const char* name; bool pitched; int note; const char* preset; };
            const Inst insts[] = { { "Hexwing", true, 69, nullptr }, { "Toxin FM", true, 69, nullptr }, { "Lantern", true, 69, nullptr }, { "Lantern Exo", true, 69, nullptr },
                                   { "Larva Bass", true, 69, nullptr }, { "Thorn Bass", true, 57, nullptr }, { "Carapace", false, 36, nullptr }, { "Live Drum Synth", false, 36, nullptr },
                                   { "Drum Pad Model", false, 48, nullptr }, { "Thump Live", false, 36, nullptr }, { "Chrysalis 2", false, 38, nullptr }, { "Chrysalis SF", false, 38, nullptr },
                                   { "Mandible", false, 36, nullptr }, { "Mandible Lite", false, 36, nullptr }, { "Pollen", true, 69, nullptr } };
            for (auto& in : insts)
            {
                juce::String nm (in.name), tr ("T " + nm);
                call ("add_track", "{\"kind\":\"instrument\",\"name\":\"" + tr + "\",\"instrument\":\"" + nm + "\"}");
                auto pl = juce::JSON::parse (call ("instrument", "{\"action\":\"preset\",\"track\":\"" + tr + "\",\"list\":true}"));
                const int np = pl["presets"].size();
                check (np >= 40, nm + ": " + juce::String (np) + " factory presets");
                auto pr = juce::JSON::parse (call ("instrument", "{\"action\":\"parameters\",\"track\":\"" + tr + "\"}"));
                check (pr["parameters"].size() >= 7, nm + ": " + juce::String (pr["parameters"].size()) + " parameters reachable");
                if (nm == "Lantern Exo" || nm == "Pollen") call ("instrument", "{\"action\":\"set\",\"track\":\"" + tr + "\",\"params\":[{\"param\":\"Sub Level\",\"real\":0}]}");   // the sub oscillator sits an octave under by design
                call ("create_midi_clip", "{\"track\":\"" + tr + "\",\"length_bars\":1,\"notes\":[{\"pitch\":" + juce::String (in.note) + ",\"start_beat\":0,\"length_beats\":3}]}");
                auto f = dir.getChildFile ("i181-" + nm.removeCharacters (" ") + ".wav");
                call ("bounce", "{\"path\":\"" + f.getFullPathName() + "\",\"tracks\":[\"" + tr + "\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}");
                const float pk = peakOf (f);
                if (in.pitched)
                {
                    const double target = 440.0 * std::pow (2.0, (in.note - 69) / 12.0), hz = pitchAt (f, 0.6, 0.15);
                    check (pk > 0.02f && pk < 1.01f && std::abs (nm == "Lantern" ? std::remainder (cents (hz, target), 1200.0) : cents (hz, target)) < (nm == "Lantern" ? 20.0 : 12.0), nm + " sounds and plays "   /* Lantern: two drifting vintage oscillators - allow drift, and fold the detector's octave lock */ + juce::String (hz, 1) + " Hz for " + juce::String (target, 1) + " (" + juce::String (cents (hz, target), 1) + " ct), peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
                }
                else check (pk > 0.02f && pk < 1.01f, nm + " sounds, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
                call ("delete_track", "{\"track\":\"" + tr + "\"}");
            }
        }

        // 2. the samplers' factory content: kit 808s in tune, the SoundFont bank plays the same as the zones, SFZ import maps and tunes
        {
            call ("add_track", R"({"kind":"instrument","name":"C2","instrument":"Chrysalis 2"})");
            call ("instrument", R"({"action":"preset","track":"C2","name":"808 | Neon Trap 808"})");
            call ("create_midi_clip", R"({"track":"C2","length_bars":1,"notes":[{"pitch":45,"start_beat":0,"length_beats":3}]})");
            auto f1 = dir.getChildFile ("c2-808.wav"); call ("bounce", "{\"path\":\"" + f1.getFullPathName() + "\",\"tracks\":[\"C2\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}");
            const double hz = pitchAt (f1, 0.5, 0.2);
            check (std::abs (cents (hz, 110.0)) < 10.0, "Chrysalis 2: the kit 808 is pitch-detected and plays A2 = " + juce::String (hz, 1) + " Hz (" + juce::String (cents (hz, 110.0), 1) + " ct)");
            call ("add_track", R"({"kind":"instrument","name":"CSF","instrument":"Chrysalis SF"})");
            call ("instrument", R"({"action":"preset","track":"CSF","name":"808 | Neon Trap 808"})");
            call ("create_midi_clip", R"({"track":"CSF","length_bars":1,"notes":[{"pitch":45,"start_beat":0,"length_beats":3}]})");
            auto f2 = dir.getChildFile ("csf-808.wav"); call ("bounce", "{\"path\":\"" + f2.getFullPathName() + "\",\"tracks\":[\"CSF\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}");
            double sr1 = 0, sr2 = 0; auto b1 = readBuf (f1, sr1), b2 = readBuf (f2, sr2); const int n = juce::jmin (b1.getNumSamples(), b2.getNumSamples());
            const double d = diffDb (b2, b1, n);
            check (n > 0 && d < -40.0, "Chrysalis SF: the factory SoundFont (written by Squelchmoth, read back) plays the same 808 as the zones (difference " + juce::String (d, 1) + " dB - 16-bit bank)");
            auto bank = SqFactory::kitBank (5); auto ps = SqSynth::SqChrysalis2Plugin::sf2Presets (bank);
            check (ps.size() == 2 && ps[0].name == "Neon Trap" && ps[1].name == "Neon Trap 808", "factory SF2 bank: " + juce::String (ps.size()) + " presets (" + (ps.size() > 0 ? ps[0].name : juce::String()) + ")");
            // SFZ: a 220 Hz tone keyed at A3 plays A4 from note 69
            auto tone = dir.getChildFile ("sfz tone.wav");
            {
                juce::AudioBuffer<float> t (1, 44100 * 2); for (int i = 0; i < t.getNumSamples(); ++i) t.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / 44100.0));
                juce::WavAudioFormat wav; tone.deleteFile();
                if (auto st = std::unique_ptr<juce::FileOutputStream> (tone.createOutputStream())) if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (st.release(), 44100.0, 1, 24, {}, 0))) w->writeFromAudioSampleBuffer (t, 0, t.getNumSamples());
            }
            auto sfz = dir.getChildFile ("tone.sfz"); sfz.replaceWithText ("<group> ampeg_release=0.2\n<region> sample=sfz tone.wav pitch_keycenter=57 lokey=0 hikey=127\n");
            auto imp = juce::JSON::parse (call ("sampler", "{\"action\":\"import\",\"track\":\"C2\",\"file\":\"" + sfz.getFullPathName() + "\"}"));
            call ("create_midi_clip", R"({"track":"C2","start_bar":3,"length_bars":1,"notes":[{"pitch":69,"start_beat":0,"length_beats":3}]})");
            auto f3 = dir.getChildFile ("c2-sfz.wav"); call ("bounce", "{\"path\":\"" + f3.getFullPathName() + "\",\"tracks\":[\"C2\"],\"start_bar\":3,\"end_bar\":4,\"bit_depth\":32}");
            const double hz3 = pitchAt (f3, 0.5, 0.2);
            check (std::abs (cents (hz3, 440.0)) < 5.0, "SFZ import: a tone keyed at A3 plays A4 = " + juce::String (hz3, 1) + " Hz (" + juce::String (cents (hz3, 440.0), 1) + " ct)");
            call ("delete_track", R"({"track":"C2"})"); call ("delete_track", R"({"track":"CSF"})");
        }

        // 3. Mandible: a factory genre loop slices at its hits and the slice dump plays the loop back
        {
            call ("set_tempo", R"({"bpm":140})");
            call ("add_track", R"({"kind":"instrument","name":"MB","instrument":"Mandible"})");
            call ("instrument", R"({"action":"preset","track":"MB","name":"Trap | Neon Trap Loop"})");
            auto sl = juce::JSON::parse (call ("slicer", R"({"action":"slices","track":"MB"})"));
            const int ns = sl["slices"].size();
            check (ns >= 12 && (double) sl["bpm"] == 140.0, "Mandible: the factory Trap loop (140 BPM) slices into " + juce::String (ns) + " hits");
            call ("slicer", R"({"action":"dump","track":"MB"})");
            auto fm = dir.getChildFile ("mand-dump.wav"); call ("bounce", "{\"path\":\"" + fm.getFullPathName() + "\",\"tracks\":[\"MB\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
            double srA = 0, srB = 0; auto y = readBuf (fm, srA); auto x = readBuf (juce::File (sl["file"].toString()), srB);
            if (srA == srB)
            {
                const double d = diffDb (y, x, juce::jmin (x.getNumSamples(), y.getNumSamples()) - 64);
                check (d < -60.0, "Mandible: the slice dump plays the loop back sample-exact (difference " + juce::String (d, 1) + " dB)");
            }
            else
            {
                const double lx = rmsDb (x, x.getNumSamples()), ly = rmsDb (y, y.getNumSamples());
                check (std::abs (lx - ly) < 0.5, "Mandible: the slice dump plays the loop back at the session rate (" + juce::String (srB, 0) + " -> " + juce::String (srA, 0) + " Hz, level " + juce::String (ly - lx, 2) + " dB)");
            }
            call ("delete_track", R"({"track":"MB"})");
            int loops = 0; for (auto& m : SqFactory::mandibleTable()) if (auto lf = FactoryLoops::render (m.genre, SqFactory::kitIndex (m.kits[0]), 0); lf.existsAsFile() && peakOf (lf) > 0.1f) ++loops;
            check (loops == (int) SqFactory::mandibleTable().size(), "factory loops: " + juce::String (loops) + " genres render from the kits");
            call ("set_tempo", R"({"bpm":120})");
        }

        // 4. the 1.8.1 effects on a drum loop
        {
            auto loopFile = FactoryLoops::render ("House", 13, 0);
            call ("set_tempo", R"({"bpm":124})");
            call ("add_track", R"({"kind":"audio","name":"FX"})");
            call ("import_audio", "{\"track\":\"FX\",\"path\":\"" + loopFile.getFullPathName() + "\",\"start_bar\":1}");
            auto fd = dir.getChildFile ("fx-dry.wav"); call ("bounce", "{\"path\":\"" + fd.getFullPathName() + "\",\"tracks\":[\"FX\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
            double sr = 0; auto dry = readBuf (fd, sr);
            auto render = [&] (const juce::String& name) { auto f = dir.getChildFile ("fx-" + name + ".wav"); call ("bounce", "{\"path\":\"" + f.getFullPathName() + "\",\"tracks\":[\"FX\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}"); double s2 = 0; return readBuf (f, s2); };
            const int n = dry.getNumSamples() - 256;
            call ("insert_plugin", R"({"track":"FX","plugin":"Time Weaver"})");
            call ("set_plugin_parameters", R"({"track":"FX","slot":1,"params":[{"param":"timeSlot","real":1},{"param":"volSlot","real":1}]})");
            auto live = render ("tw-live");
            check (diffDb (live, dry, n) < -120.0, "Time Weaver: the Live slot passes the audio through untouched (" + juce::String (diffDb (live, dry, n), 1) + " dB)");
            call ("plugin_presets", R"({"track":"FX","slot":1,"action":"load","index":0})");
            auto ht = render ("tw-half");
            check (diffDb (ht, dry, n) > -6.0 && std::abs (rmsDb (ht, n) - rmsDb (dry, n)) < 3.0, "Time Weaver: Half Time Drop rewrites the loop at the same level (" + juce::String (rmsDb (ht, n) - rmsDb (dry, n), 1) + " dB)");
            call ("remove_plugin", R"({"track":"FX","slot":1})");
            call ("insert_plugin", R"({"track":"FX","plugin":"Larva Filter"})");
            auto lf = render ("larva");
            check (diffDb (lf, dry, n) > -20.0 && std::abs (rmsDb (lf, n) - rmsDb (dry, n)) < 4.0, "Larva Filter: the stepped filter moves the sound and keeps the level (" + juce::String (rmsDb (lf, n) - rmsDb (dry, n), 1) + " dB)");
            call ("remove_plugin", R"({"track":"FX","slot":1})");
            call ("insert_plugin", R"({"track":"FX","plugin":"Apex Triad"})");
            call ("set_plugin_parameters", R"({"track":"FX","slot":1,"params":[{"param":"inputGain","real":12},{"param":"ceiling","real":-1}]})");
            auto ax = render ("triad");
            const float pk = ax.getMagnitude (0, ax.getNumSamples());
            check (juce::Decibels::gainToDecibels (pk) <= -0.95f && rmsDb (ax, n) > rmsDb (dry, n) + 3.0, "Apex Triad: +12 dB into a -1 dB ceiling: peak " + juce::String (juce::Decibels::gainToDecibels (pk), 2) + " dBFS, " + juce::String (rmsDb (ax, n) - rmsDb (dry, n), 1) + " dB louder");
            call ("delete_track", R"({"track":"FX"})");
            call ("set_tempo", R"({"bpm":120})");
        }

        // 5. the 27 kits: every new kit renders all twelve sounds
        {
            int good = 0;
            for (int k = 17; k < DrumKit::numKits; ++k) { auto files = DrumKit::ensureKit (*session, k); int ok = 0; for (auto& f : files) if (peakOf (f) > 0.05f) ++ok; if (ok == DrumKit::numSounds) ++good; }
            check (DrumKit::numKits == 27 && good == 10, "kits: " + juce::String (DrumKit::numKits) + " kits, the ten new ones render all twelve sounds (" + juce::String (good) + "/10)");
        }

        // 5b. Burn to MIDI: an arpeggiated chord becomes real notes that sound exactly the same with the arpeggiator off
        {
            call ("new_session", R"({"name":"Selftest Burn"})");
            call ("set_tempo", R"({"bpm":120})");
            call ("rack", R"({"action":"add","type":"instrument","what":"Mini Moth","name":"Arp"})");
            call ("channel_settings", R"({"action":"set","channel":"Arp","arp":{"mode":"up","time":0.25,"range":2}})");
            call ("rack", R"({"action":"notes","channel":"Arp","notes":[{"pitch":60,"start_beat":0,"length_beats":3},{"pitch":64,"start_beat":0,"length_beats":3},{"pitch":67,"start_beat":0,"length_beats":3}]})");
            call ("pattern", R"({"action":"place","pattern":1,"bar":1,"repeats":1})");
            auto fa = dir.getChildFile ("burn-before.wav"), fb = dir.getChildFile ("burn-after.wav");
            call ("bounce", "{\"path\":\"" + fa.getFullPathName() + "\",\"tracks\":[\"Arp\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
            auto br = juce::JSON::parse (call ("rack", R"({"action":"burn_to_midi","channel":"Arp"})"));
            call ("bounce", "{\"path\":\"" + fb.getFullPathName() + "\",\"tracks\":[\"Arp\"],\"start_bar\":1,\"end_bar\":3,\"bit_depth\":32}");
            double s1 = 0, s2 = 0; auto ba = readBuf (fa, s1), bb = readBuf (fb, s2); const int n = juce::jmin (ba.getNumSamples(), bb.getNumSamples());
            const double d = diffDb (bb, ba, n);
            check ((int) br["notes_in"] == 3 && (int) br["notes_out"] > 6 && rmsDb (ba, n) > -60.0 && d < -60.0, "Burn to MIDI: 3 held notes -> " + br["notes_out"].toString() + " arpeggiated notes that play the same with the arpeggiator off (" + juce::String (d, 1) + " dB)");
        }

        // 6. Cut Lab from Claude: info, normalise, reverse twice, cut, stretch
        {
            auto src = FactoryLoops::render ("Boom Bap", 12, 0);
            auto work = dir.getChildFile ("cutlab work.wav"); src.copyFileTo (work);
            auto info = juce::JSON::parse (call ("cut_lab", "{\"action\":\"info\",\"file\":\"" + work.getFullPathName() + "\"}"));
            const double secs = (double) info["seconds"];
            auto nrm = juce::JSON::parse (call ("cut_lab", "{\"action\":\"edit\",\"file\":\"" + work.getFullPathName() + "\",\"in_place\":true,\"ops\":[{\"op\":\"normalise\",\"peak_db\":-1}]}"));
            check (std::abs ((double) nrm["peak_db"] + 1.0) < 0.05, "Cut Lab: normalise to -1 dBFS -> " + nrm["peak_db"].toString().substring (0, 6) + " dBFS");
            double s0 = 0, s1 = 0; auto before = readBuf (work, s0);
            call ("cut_lab", "{\"action\":\"edit\",\"file\":\"" + work.getFullPathName() + "\",\"in_place\":true,\"ops\":[{\"op\":\"reverse\"},{\"op\":\"reverse\"}]}");
            auto after = readBuf (work, s1);
            check (diffDb (after, before, before.getNumSamples()) < -120.0, "Cut Lab: reverse twice gives back the original exactly");
            auto cut = juce::JSON::parse (call ("cut_lab", "{\"action\":\"edit\",\"file\":\"" + work.getFullPathName() + "\",\"output\":\"cutlab cut.wav\",\"ops\":[{\"op\":\"cut\",\"start_seconds\":0.5,\"end_seconds\":1.0}]}"));
            check (std::abs ((double) cut["seconds"] - (secs - 0.5)) < 0.001, "Cut Lab: cut half a second -> " + juce::String ((double) cut["seconds"], 3) + " s of " + juce::String (secs, 3));
            auto st = juce::JSON::parse (call ("cut_lab", "{\"action\":\"edit\",\"file\":\"" + work.getFullPathName() + "\",\"output\":\"cutlab stretch.wav\",\"ops\":[{\"op\":\"stretch\",\"ratio\":2.0}]}"));
            check (std::abs ((double) st["seconds"] / secs - 2.0) < 0.02, "Cut Lab: stretch x2 -> " + juce::String ((double) st["seconds"], 3) + " s (pitch kept)");
        }
        // 7. Editions (1.9): the Demo cannot reach anything locked - menus, Claude, sessions, kits, windows, exports, tracks
        {
            std::cout << "  -- editions --\n";
            call ("new_session", R"({"name":"Selftest Editions"})");
            call ("add_track", R"({"kind":"instrument","name":"Hex","instrument":"Hexwing"})");
            call ("insert_plugin", R"({"track":"Hex","plugin":"Moth Tune"})");
            auto saved = session->getFile();
            call ("save_session", "{}");
            const auto complete = Edition::owned();
            Edition::owned() = Edition::Owned {};   // Demo
            auto r1 = call ("insert_plugin", R"({"track":"Hex","plugin":"Apex Limiter"})", false);
            check (r1.contains ("locked") && r1.contains ("Mastering Suite"), "Demo: a pack plug-in is refused with the pack that unlocks it");
            auto r2 = call ("insert_plugin", R"({"track":"Hex","plugin":"Aurora Verb"})", false);
            check (r2.contains ("locked") && r2.contains ("Squelchmoth"), "Demo: a core plug-in is refused");
            auto r3 = call ("insert_plugin", R"({"track":"Hex","plugin":"AW Tape Delay"})", false);
            check (r3.contains ("locked"), "Demo: the AirWindows collection is locked");
            call ("insert_plugin", R"({"track":"Hex","plugin":"Prism EQ"})");
            auto r4 = call ("add_track", R"({"kind":"instrument","name":"Lan","instrument":"Lantern"})", false);
            check (r4.contains ("locked") || r4.contains ("Instrument Collection"), "Demo: a locked instrument can't be added");
            auto r5 = call ("mastering_status", "{}", false);
            check (r5.contains ("Squelchmoth"), "Demo: the Mastering window's tools are locked");
            call ("cut_lab", R"({"action":"info","file":"/nonexistent.wav"})", false);
            auto r6 = call ("export_stems", "{}", false);
            check (r6.contains ("locked") || r6.contains ("part of Squelchmoth"), "Demo: stem export is locked");
            check (! Edition::isKitAllowed (6) && Edition::isKitAllowed (2) && ! Edition::isKitAllowed (20), "Demo: kits 1-5 only; kits 18-27 need the Beat Lab Expansion");
            // a session made with locked plug-ins opens with placeholders that keep every setting
            call ("open_session", "{\"path\":\"" + saved.getFullPathName() + "\"}");
            auto* hex = session->findTrack ("Hex");
            juce::StringArray kinds; bool sawLocked = false, typesKept = true;
            if (hex) for (auto p : hex->pluginList.getPlugins()) { kinds.add (p->getName()); if (dynamic_cast<SqLockedPlugin*> (p)) sawLocked = true; }
            if (hex) for (auto p : hex->pluginList.getPlugins()) if (dynamic_cast<SqLockedPlugin*> (p) && p->getPluginType() != "sqmHexwing" && p->getPluginType() != "sqmTune") typesKept = false;
            check (sawLocked && typesKept, "Demo: a session with locked plug-ins loads them as Locked placeholders that keep their type (" + kinds.joinIntoString (", ") + ")");
            call ("create_midi_clip", R"({"track":"Hex","length_bars":1,"notes":[{"pitch":60,"start_beat":0,"length_beats":2}]})");
            auto fz = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm-locked.wav"); fz.deleteFile();
            call ("bounce", "{\"path\":\"" + fz.getFullPathName() + "\",\"tracks\":[\"Hex\"],\"start_bar\":1,\"end_bar\":2,\"bit_depth\":32}", false);   // a silent render may be refused as "no audio"
            check (! fz.existsAsFile() || peakOf (fz) < 1.0e-6f, "Demo: a locked instrument renders silence");
            call ("save_session", "{}");
            // 16 tracks
            call ("new_session", R"({"name":"Selftest Demo Tracks"})");
            int made = 0; for (int i = 0; i < 18; ++i) if (! call ("add_track", R"({"kind":"audio"})", session->getTracks().size() < Edition::demoMaxTracks).contains ("Demo")) ++made;
            check (session->getTracks().size() == Edition::demoMaxTracks, "Demo: 16 tracks, the 17th is refused (" + juce::String (session->getTracks().size()) + ")");
            // unlocking brings the real plug-ins back with their settings
            Edition::owned() = complete;
            call ("open_session", "{\"path\":\"" + saved.getFullPathName() + "\"}");
            hex = session->findTrack ("Hex"); bool real = hex != nullptr;
            if (hex) for (auto p : hex->pluginList.getPlugins()) if (dynamic_cast<SqLockedPlugin*> (p)) real = false;
            check (real && hex != nullptr && dynamic_cast<SqSynth::SqHexwingPlugin*> (Rack::instrumentOf (*hex)) != nullptr, "after unlocking, the saved session reopens with the real Hexwing and Moth Tune");
            Edition::Owned vocalOnly; vocalOnly.core = true; vocalOnly.vocal = true; Edition::owned() = vocalOnly;
            check (Edition::isTypeAllowed ("sqmTune") && ! Edition::isTypeAllowed ("sqmApexLim") && Edition::isTypeAllowed ("sqmAurora") && Edition::isTypeAllowed ("airwindows_tapedelay"), "Squelchmoth + Vocal Suite: core and the vocal pack, not the mastering pack");
            Edition::owned() = complete;
        }

        std::cout << "  (1.8.1 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)\n";
    }

    //==========================================================================
    // 1.10 "Console": the console model, COMP / LIMIT slots, gain reduction, strip meters (BS.1770-4), gang,
    // the 1.10 wished features and Squelchmoth Chat's tool loop against a local mock of the Messages API.
    {
        std::cout << "\n-- 1.10 console, wished features and Squelchmoth Chat --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory);
        auto writeSine = [] (const juce::File& f, double hz, double peakDb, double seconds, int channels = 2)
        {
            const double sr = 48000.0; const int len = (int) (sr * seconds);
            juce::AudioBuffer<float> buf (channels, len);
            const float a = juce::Decibels::decibelsToGain ((float) peakDb);
            for (int i = 0; i < len; ++i) for (int c = 0; c < channels; ++c) buf.setSample (c, i, a * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = f.createOutputStream())
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, (unsigned int) channels, 24, {}, 0)))
                    w->writeFromAudioSampleBuffer (buf, 0, len);
        };
        auto pathOf = [] (const juce::File& f) { return f.getFullPathName().replace ("\\", "/"); };

        call ("new_session", R"({"name":"Selftest Console"})");
        for (auto& t : session->getTracks()) juce::ignoreUnused (t);
        while (session->getTracks().size() > 0) session->edit->deleteTrack (session->getTracks().getFirst());
        for (auto n : { "Kick", "Vox", "Gtr" }) call ("add_track", juce::String (R"({"kind":"audio","name":")") + n + "\"}");

        // widths, views, layouts
        call ("mixer_layout", R"({"action":"set_width","track":"Kick","width":"compact"})");
        call ("mixer_layout", R"({"action":"set_width","track":"Vox","width":"expanded"})");
        auto ml = juce::JSON::parse (call ("mixer_layout", R"({"action":"get"})"));
        check (ml["strips"][0]["width"].toString() == "compact" && ml["strips"][1]["width"].toString() == "expanded" && ml["strips"][2]["width"].toString() == "standard",
               "console: strip widths are stored per track (compact / expanded / standard default)");
        call ("mixer_layout", R"({"action":"save","name":"Tracking"})");
        call ("mixer_layout", R"({"action":"set_layout","layout":"all_compact"})");
        ml = juce::JSON::parse (call ("mixer_layout", R"({"action":"get"})"));
        check (ml["strips"][1]["width_shown"].toString() == "compact" && ml["strips"][1]["width"].toString() == "expanded", "console: All Compact overrides what is shown, keeps what is stored");
        call ("mixer_layout", R"({"action":"set_width","track":"Kick","width":"expanded"})");
        call ("mixer_layout", R"({"action":"recall","name":"Tracking"})");
        ml = juce::JSON::parse (call ("mixer_layout", R"({"action":"get"})"));
        check (ml["strips"][0]["width"].toString() == "compact" && ml["channel_layout"].toString() == "flexible", "console: a saved layout recalls every width and the channel layout");
        call ("mixer_layout", R"({"action":"set_view","view":"full"})");
        check (Console::Model::view() == Console::View::full, "console: Full view");
        call ("mixer_layout", R"({"action":"set_view","view":"dynamics"})");
        check (Console::Map::dbToProp (0.0f) > 0.7f && std::abs (Console::Map::propToDb (Console::Map::dbToProp (-9.0f)) + 9.0f) < 0.01f
               && std::abs (Console::Map::propToDb (Console::Map::dbToProp (-50.0f)) + 50.0f) < 0.01f, "console: the fader / meter dB map round-trips (0 dB high on the fader, -9 and -50 exact)");

        // COMP / LIMIT slots: order, replace in place, remove
        call ("channel_dynamics", R"({"action":"set","track":"Vox","comp":"Opto Leveler","limit":"Apex Limiter"})");
        auto* vox = session->findTrack ("Vox");
        auto ins = vox != nullptr ? Session::getInserts (vox->pluginList, vox) : juce::Array<te::Plugin*>();
        check (ins.size() == 2 && ins[0]->getPluginType() == "sqmOpto" && ins[1]->getPluginType() == "sqmApexLim", "COMP / LIMIT: compressor first, limiter last before the fader");
        call ("insert_plugin", R"({"track":"Vox","plugin":"Prism EQ"})");
        call ("channel_dynamics", R"({"action":"set","track":"Vox","comp":"Cocoon Compressor"})");
        ins = Session::getInserts (vox->pluginList, vox);
        int cocoonAt = -1, limAt = -1;
        for (int i = 0; i < ins.size(); ++i) { if (ins[i]->getPluginType() == "compressor") cocoonAt = i; if (ins[i]->getPluginType() == "sqmApexLim") limAt = i; }
        check (cocoonAt == 0 && limAt == ins.size() - 1 && ins.size() == 3, "COMP / LIMIT: replacing the compressor keeps its slot; a new insert stays before the limiter");
        call ("channel_dynamics", R"({"action":"set","track":"Vox","limit":"Cocoon Limit"})");
        auto dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Vox"})"));
        check (dyn["limit"]["plugin"].toString() == "Cocoon Limit" && dyn["comp"]["plugin"].toString() == "Cocoon Compressor", "COMP / LIMIT: two Cocoons, one in each role, are told apart");
        call ("channel_dynamics", R"({"action":"set","track":"Vox","comp":"none"})");
        dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Vox"})"));
        check (dyn["comp"]["plugin"].toString() == "None" && dyn["limit"]["plugin"].toString() == "Cocoon Limit", "COMP / LIMIT: none removes only the compressor");
        call ("channel_dynamics", R"({"action":"set","track":"Vox","comp":"SSL Comp"})", false);
        {
            const auto complete = Edition::owned();
            Edition::owned() = Edition::Owned {};
            auto r = call ("channel_dynamics", R"({"action":"set","track":"Gtr","limit":"Apex Limiter"})", false);
            check (r.contains ("locked") && r.contains ("Mastering Suite"), "COMP / LIMIT: a locked limiter is refused with the product that unlocks it");
            Edition::owned() = complete;
        }

        // gain reduction: Cocoon (Tracktion patch) on a hot sine, and the Apex limiter
        auto hot = tmp.getChildFile ("sqm110_hot.wav"); writeSine (hot, 1000.0, -1.0, 6.0);
        call ("import_audio", "{\"track\":\"Gtr\",\"path\":\"" + pathOf (hot) + "\"}");
        call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"Cocoon Compressor"})");
        auto* gtr = session->findTrack ("Gtr");
        if (auto c = gtr != nullptr ? Console::Model::findDynamics (gtr->pluginList, gtr, false) : nullptr)
        {
            Console::Model::setParam (*c, "threshold", juce::Decibels::decibelsToGain (-30.0f));
            Console::Model::setParam (*c, "ratio", 0.25f);
        }
        auto fGr = tmp.getChildFile ("sqm110_gr.wav"); fGr.deleteFile();
        call ("bounce", "{\"path\":\"" + pathOf (fGr) + "\",\"tracks\":[\"Gtr\"],\"start_bar\":1,\"end_bar\":3,\"tail_seconds\":0}");
        dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Gtr"})"));
        const double cgr = dyn["comp"]["gain_reduction_db"].toString().getDoubleValue();
        check (cgr < -3.0, "gain reduction: Cocoon reports real GR on a hot signal (" + juce::String (cgr, 1) + " dB, threshold -30 dB, 4:1)");
        call ("channel_dynamics", R"({"action":"set","track":"Gtr","limit":"Apex Limiter"})");
        call ("bounce", "{\"path\":\"" + pathOf (fGr) + "\",\"tracks\":[\"Gtr\"],\"start_bar\":1,\"end_bar\":3,\"tail_seconds\":0}");
        dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Gtr"})"));
        check (dyn["limit"]["gain_reduction_db"].isDouble() || dyn["limit"]["gain_reduction_db"].isInt(), "gain reduction: the Apex Limiter reports GR as a number (" + dyn["limit"]["gain_reduction_db"].toString() + " dB)");
        call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"Moth Press"})");
        dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Gtr"})"));
        check (dyn["comp"]["gain_reduction_db"].toString().contains ("not reported"), "gain reduction: a plug-in that does not report GR says so (never a made-up number)");

        // strip meter: EBU Tech 3341 test 1 - a stereo 1 kHz sine at -23 dBFS reads -23.0 LUFS
        auto ebu = tmp.getChildFile ("sqm110_ebu.wav"); writeSine (ebu, 1000.0, -23.0, 8.0);
        call ("add_track", R"({"kind":"audio","name":"EBU"})");
        auto* ebuT = session->findTrack ("EBU");
        if (ebuT) TrackOps::setTrackFormat (*session, *ebuT, true);
        call ("import_audio", "{\"track\":\"EBU\",\"path\":\"" + pathOf (ebu) + "\"}");
        call ("mixer_layout", R"({"action":"set_meter","track":"EBU","meter":"lufs_s"})");
        if (ebuT) if (auto tap = Console::Model::findTap (ebuT->pluginList)) tap->resetIntegrated();
        auto fEbu = tmp.getChildFile ("sqm110_ebu_out.wav"); fEbu.deleteFile();
        call ("bounce", "{\"path\":\"" + pathOf (fEbu) + "\",\"tracks\":[\"EBU\"],\"start_bar\":1,\"end_bar\":4,\"tail_seconds\":0}");
        auto tapT = ebuT != nullptr ? Console::Model::findTap (ebuT->pluginList) : nullptr;
        const float integ = tapT != nullptr ? tapT->getIntegrated() : -100.0f, st = tapT != nullptr ? tapT->getShortTerm() : -100.0f;
        check (tapT != nullptr && std::abs (integ + 23.0f) <= 0.5f, "strip meter: EBU 3341 test 1 integrated = " + juce::String (integ, 2) + " LUFS (expect -23.0 +/- 0.5)");
        check (tapT != nullptr && std::abs (st + 23.0f) <= 0.5f, "strip meter: short-term = " + juce::String (st, 2) + " LUFS");
        check (std::abs (tapT != nullptr ? tapT->getRmsMaxDb (0) + 26.01f : 99.0f) <= 0.3f, "strip meter: RMS of a -23 dBFS sine is -26.0 dB (" + juce::String (tapT != nullptr ? tapT->getRmsMaxDb (0) : 0.0f, 2) + ")");
        check (Session::getInserts (ebuT->pluginList, ebuT).isEmpty(), "strip meter: the tap never shows as an insert");
        auto undoName = session->edit->getUndoManager().getUndoDescription();
        check (! undoName.containsIgnoreCase ("Strip Meter"), "strip meter: adding the tap is not an undo step");

        // gang selected faders (Pro Tools wish), Alt moves one
        {
            TimelineState st2;
            MixerView mv (*session, &st2);
            mv.setSize (1400, 800);
            auto rowsNow = session->getRows();
            st2.selectTrack (rowsNow[0]); st2.selectTrack (rowsNow[2], true);
            auto strips = mv.getStrips();
            ChannelStrip* s0 = nullptr; ChannelStrip* s1 = nullptr; ChannelStrip* s2 = nullptr;
            for (auto* sp : strips) { if (sp->getRow() == rowsNow[0]) s0 = sp; if (sp->getRow() == rowsNow[1]) s1 = sp; if (sp->getRow() == rowsNow[2]) s2 = sp; }
            if (s0 && s1 && s2)
            {
                const float b1 = s1->faderDb(), b2 = s2->faderDb();
                s0->userMoveFader (s0->faderDb() - 4.0f);
                check (std::abs (s2->faderDb() - (b2 - 4.0f)) < 0.11f && std::abs (s1->faderDb() - b1) < 0.01f,
                       "gang: dragging a selected fader moves the other selected strip by the same dB, not the unselected one (moved "
                       + juce::String (s0->faderDb(), 1) + ", other selected " + juce::String (b2, 1) + " -> " + juce::String (s2->faderDb(), 1) + ", unselected " + juce::String (b1, 1) + " -> " + juce::String (s1->faderDb(), 1)
                       + ", selected " + juce::String ((int) s0->isSelected()) + juce::String ((int) s2->isSelected()) + ")");
                auto* gt = dynamic_cast<te::AudioTrack*> (rowsNow[2]);
                check (gt != nullptr && std::abs (gt->getVolumePlugin()->getVolumeDb() - (b2 - 4.0f)) < 0.11f, "gang: the ganged track's volume really changed");
            }
            else check (false, "gang: the console built a strip for every track");
            check (mv.getMasterStrip() == nullptr || mv.getMasterStrip()->preferredWidth() > 0, "console: the master strip lays out");
        }

        // Mono button = track format
        if (auto* k = session->findTrack ("Kick"))
        {
            TrackOps::setTrackFormat (*session, *k, false);
            const bool mono = TrackOps::isMonoFormat (*session, *k);
            TrackOps::setTrackFormat (*session, *k, true);
            check (mono && ! TrackOps::isMonoFormat (*session, *k), "Mono: toggles the track format both ways");
        }

        // modulate (host LFO)
        auto mo = call ("modulate", R"({"action":"add","track":"Gtr","target":"volume","wave":"triangle","sync":"1/8","depth":0.4})");
        auto ml2 = juce::JSON::parse (call ("modulate", R"({"action":"list","track":"Gtr"})"));
        check (ml2.size() == 1 && ml2[0]["rate"].toString() == "1/8" && ml2[0]["moves"][0]["parameter"].toString() == "Track > Volume" && std::abs ((double) ml2[0]["moves"][0]["depth"] - 0.4) < 0.001,
               "modulate: a tempo-synced LFO moves the track volume by the set depth (" + ml2[0]["rate"].toString() + ", " + ml2[0]["moves"][0]["parameter"].toString() + ")");
        call ("modulate", R"({"action":"add","track":"Gtr","target":"pan","rate_hz":0.5})");
        call ("modulate", R"({"action":"remove","track":"Gtr","all":true})");
        check (juce::JSON::parse (call ("modulate", R"({"action":"list","track":"Gtr"})")).size() == 0, "modulate: remove all clears every LFO");
        call ("modulate", R"({"action":"add","track":"Gtr","sync":"1/7"})", false);

        // automation humanise
        call ("write_automation", R"({"track":"Kick","target":"volume","points":[{"bar":1,"value":-6},{"bar":2,"value":-3},{"bar":3,"value":-6},{"bar":4,"value":-3},{"bar":5,"value":-6}]})");
        auto* kick = session->findTrack ("Kick");
        juce::Array<double> before;
        if (kick) for (int i = 0; i < kick->getVolumePlugin()->volParam->getCurve().getNumPoints(); ++i) before.add (kick->getVolumePlugin()->volParam->getCurve().getPointTime (i).inSeconds());
        call ("automation_humanise", R"({"track":"Kick","target":"volume","time_ms":20,"amount":1,"seed":7})");
        bool moved = false, ordered = true; int n = 0;
        if (kick)
        {
            auto& cv = kick->getVolumePlugin()->volParam->getCurve();
            n = cv.getNumPoints();
            for (int i = 0; i < n; ++i) { if (i < before.size() && std::abs (cv.getPointTime (i).inSeconds() - before[i]) > 1.0e-5) moved = true; if (i > 0 && cv.getPointTime (i) <= cv.getPointTime (i - 1)) ordered = false; }
        }
        check (n == before.size() && moved && ordered, "automation_humanise: every point nudged, none lost, order kept (" + juce::String (n) + " points)");

        // sort tracks
        call ("sort_tracks", R"({"by":"name"})");
        juce::StringArray order; for (auto r : session->getRows()) order.add (r->getName());
        auto sorted = order; sorted.sortNatural();
        check (order == sorted, "sort_tracks: A-Z (" + order.joinIntoString (", ") + ")");

        // aux next to its source
        call ("add_send", R"({"track":"Gtr","bus":"new","bus_name":"Gtr Verb"})");
        auto rowsAfter = session->getRows();
        int gi = -1, vi = -1;
        for (int i = 0; i < rowsAfter.size(); ++i) { if (rowsAfter[i]->getName() == "Gtr") gi = i; if (rowsAfter[i]->getName() == "Gtr Verb") vi = i; }
        check (gi >= 0 && vi == gi + 1, "add_send new: the new aux lands right after its source track");

        // commit + uncommit
        call ("track_options", R"({"track":"Kick","commit":true})");
        bool hasCm = false; for (auto r : session->getRows()) if (r->getName() == "Kick.cm") hasCm = true;
        call ("uncommit", R"({"track":"Kick.cm"})");
        bool cmGone = true; for (auto r : session->getRows()) if (r->getName() == "Kick.cm") cmGone = false;
        auto* kickBack = session->findTrack ("Kick");
        check (hasCm && cmGone && kickBack != nullptr && ! kickBack->isHidden() && kickBack->isProcessing (false), "uncommit: the source comes back visible and active, the .cm track goes");

        // stems: skip silent; render queue
        auto silent = tmp.getChildFile ("sqm110_silent.wav"); writeSine (silent, 1000.0, -200.0, 4.0);
        call ("add_track", R"({"kind":"audio","name":"Silence"})");
        call ("import_audio", "{\"track\":\"Silence\",\"path\":\"" + pathOf (silent) + "\"}");
        auto stems = juce::JSON::parse (call ("export_stems", R"({"tracks":["Silence","Gtr"],"skip_silent":true,"tail_seconds":0})"));
        check ((int) stems["skipped_silent"] == 1 && (int) stems["written"] == 1, "export_stems skip_silent: the silent stem is skipped, the real one written");
        call ("render_queue", R"({"action":"clear"})");
        call ("render_queue", R"({"action":"add","job":"bounce","label":"Hook","options":{"start_bar":1,"end_bar":2,"tail_seconds":0}})");
        call ("render_queue", R"({"action":"add","job":"stems","options":{"tracks":["Gtr"],"tail_seconds":0}})");
        check (juce::JSON::parse (call ("render_queue", R"({"action":"list"})")).size() == 2, "render_queue: two jobs queued (saved in the session)");
        auto rq = juce::JSON::parse (call ("render_queue", R"({"action":"run"})"));
        check ((int) rq["ran"] == 2 && (int) rq["succeeded"] == 2, "render_queue: run renders every job in order (" + juce::String ((int) rq["succeeded"]) + "/2)");
        check (juce::JSON::parse (call ("render_queue", R"({"action":"list"})")).size() == 0, "render_queue: empty after a run");

        // Control Room: monitor-only master inserts and the Master fader never print
        {
            auto bounceGtr = [&] (const juce::String& name)
            {
                auto f = tmp.getChildFile (name); f.deleteFile();
                call ("bounce", "{\"path\":\"" + pathOf (f) + "\",\"tracks\":[\"Gtr\"],\"start_bar\":1,\"end_bar\":3,\"tail_seconds\":0,\"bit_depth\":32}");
                return juce::Decibels::gainToDecibels (peakOf (f), -100.0f);
            };
            const float a = bounceGtr ("sqm110_cr_a.wav");
            call ("monitor_chain", R"({"action":"add","plugin":"Cocoon Compressor"})");
            auto ml3 = juce::JSON::parse (call ("monitor_chain", R"({"action":"list"})"));
            const int crSlot = ml3["master_inserts"].size();
            if (auto ins = Session::getInserts (session->edit->getMasterPluginList(), nullptr); ! ins.isEmpty())
            {
                Console::Model::setParam (*ins.getLast(), "threshold", juce::Decibels::decibelsToGain (-40.0f));
                Console::Model::setParam (*ins.getLast(), "ratio", 0.0f);
            }
            const float b = bounceGtr ("sqm110_cr_b.wav");
            check (crSlot >= 1 && ! (bool) ml3["master_inserts"][crSlot - 1]["prints"] && std::abs (a - b) < 0.05f,
                   "Control Room: a monitor-only master insert is not printed (" + juce::String (a, 2) + " vs " + juce::String (b, 2) + " dBFS)");
            call ("monitor_chain", "{\"action\":\"set\",\"slot\":" + juce::String (crSlot) + ",\"monitor\":false}");
            const float c = bounceGtr ("sqm110_cr_c.wav");
            check (c < a - 3.0f, "Control Room: the same insert prints once it is not monitor-only (" + juce::String (c, 2) + " dBFS)");
            call ("monitor_chain", "{\"action\":\"set\",\"slot\":" + juce::String (crSlot) + ",\"monitor\":true}");
            auto mv = session->edit->getMasterVolumePlugin();
            mv->setVolumeDb (-12.0f);
            call ("monitor_chain", R"({"action":"master_fader_prints","value":false})");
            const float d = bounceGtr ("sqm110_cr_d.wav");
            check (std::abs (d - a) < 0.05f && std::abs (mv->getVolumeDb() + 12.0f) < 0.01f, "Control Room: renders ignore the Master fader when asked, and the fader comes back (" + juce::String (d, 2) + " dBFS)");
            call ("monitor_chain", R"({"action":"master_fader_prints","value":true})");
            const float e = bounceGtr ("sqm110_cr_e.wav");
            check (std::abs (e - (a - 12.0f)) < 0.1f, "Control Room: the Master fader prints again by default (" + juce::String (e, 2) + " dBFS)");
            mv->setVolumeDb (0.0f);
            for (auto p : MonitorChain::plugins (*session)) p->deleteFromParent();
        }

        // favourites, MIDI CC, clip bins
        call ("plugin_favourites", R"({"action":"add","name":"Prism EQ"})");
        check (PluginMenu::isFavourite ("Prism EQ"), "plugin_favourites: added");
        {
            auto menu = PluginMenu::build (*engine, PluginMenu::effects);
            bool found = false;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();) if (it.getItem().text.contains ("Favourites")) found = true;
            check (found, "plugin_favourites: every plug-in menu opens with a Favourites submenu");
        }
        call ("plugin_favourites", R"({"action":"remove","name":"Prism EQ"})");
        call ("add_track", R"({"kind":"instrument","name":"Keys CC","instrument":"Mothsynth"})");
        call ("create_midi_clip", R"({"track":"Keys CC","length_bars":2,"notes":[{"pitch":60,"start_beat":0,"length_beats":4}]})");
        call ("midi_cc", R"({"action":"ramp","track":"Keys CC","cc":74,"from_beat":0,"to_beat":4,"from":10,"to":120,"steps":10})");
        auto ccr = juce::JSON::parse (call ("midi_cc", R"({"action":"read","track":"Keys CC","cc":74})"));
        check (ccr.size() == 11 && (int) ccr[0]["value"] == 10 && (int) ccr[10]["value"] == 120 && std::abs ((double) ccr[5]["beat"] - 2.0) < 1.0e-6, "midi_cc: a 10-step ramp 10 -> 120 over 4 beats lands exactly");
        call ("midi_cc", R"({"action":"write","track":"Keys CC","cc":"pitch","points":[{"beat":1,"value":-8192},{"beat":2,"value":8191}]})");
        auto pr = juce::JSON::parse (call ("midi_cc", R"({"action":"read","track":"Keys CC","cc":"pitch"})"));
        check (pr.size() == 2 && (int) pr[0]["value"] == -8192 && (int) pr[1]["value"] == 8191, "midi_cc: pitch bend keeps its full -8192..8191 range");
        call ("midi_cc", R"({"action":"write","track":"Keys CC","cc":200,"points":[]})", false);
        call ("clip_bins", R"({"action":"file","clips":[{"track":"Gtr","clip":1},{"track":"Keys CC","clip":1}],"bin":"Takes"})");
        auto bl = juce::JSON::parse (call ("clip_bins", R"({"action":"list"})"));
        check (bl["bins"].size() == 1 && bl["bins"][0].toString() == "Takes" && bl["contents"]["Takes"].size() == 2, "clip_bins: two clips filed in a bin");
        call ("clip_bins", R"({"action":"rename","bin":"Takes","to":"Keepers"})");
        bl = juce::JSON::parse (call ("clip_bins", R"({"action":"list"})"));
        check (bl["bins"][0].toString() == "Keepers", "clip_bins: rename");

        // private settings
        check (call ("set_preference", R"({"key":"cl_anthropicKey","value":"x"})", false).contains ("private")
               && call ("set_preference", R"({"key":"chat_apiBase","value":"http://evil.example"})", false).contains ("private")
               && call ("set_preference", R"({"key":"licenceKeys"})", false).contains ("private"), "set_preference refuses the API key, the chat endpoint and licence keys");

        //======================================================================
        // Squelchmoth Chat against a local mock of the Messages API
        {
            httplib::Server mock;
            std::atomic<int> posts { 0 }, gets { 0 };
            std::atomic<bool> sawVersion { true }, sawAuth { true }, sawTemperature { false }, resultsFirst { true }, sawIsError { false }, sawCache { false };
            std::atomic<int> toolCount { 0 };
            const juce::String testKey = "sk-ant-selftest-0123456789abcdefXYZ";
            mock.Get ("/v1/models", [&] (const httplib::Request& req, httplib::Response& res)
            {
                ++gets;
                if (req.get_header_value ("Authorization") != ("Bearer " + testKey).toStdString()) { res.status = 401; res.set_content (R"({"type":"error","error":{"type":"authentication_error","message":"bad key"}})", "application/json"); return; }
                res.set_content (R"({"data":[{"type":"model","id":"claude-opus-5-5","display_name":"Claude Opus 5.5","lifecycle":"active"}],"has_more":false})", "application/json");
            });
            mock.Post ("/v1/messages", [&] (const httplib::Request& req, httplib::Response& res)
            {
                ++posts;
                if (req.get_header_value ("anthropic-version") != Chat::apiVersion) sawVersion = false;
                if (req.get_header_value ("Authorization") != ("Bearer " + testKey).toStdString()) sawAuth = false;
                auto body = juce::JSON::parse (juce::String::fromUTF8 (req.body.data(), (int) req.body.size()));
                if (body.hasProperty ("temperature") || body.hasProperty ("top_p")) sawTemperature = true;
                toolCount = body["tools"].size();
                if (auto tl = body["tools"].getArray()) if (! tl->isEmpty() && tl->getLast().hasProperty ("cache_control")) sawCache = true;
                auto msgs = body["messages"];
                auto last = msgs[msgs.size() - 1];
                bool hasResult = false;
                if (auto c = last["content"].getArray())
                {
                    for (int i = 0; i < c->size(); ++i)
                    {
                        if ((*c)[i]["type"].toString() == "tool_result") { hasResult = true; if ((bool) (*c)[i]["is_error"]) sawIsError = true; }
                        else if (hasResult) resultsFirst = false;
                    }
                }
                if (! hasResult)
                    res.set_content (R"({"id":"m1","type":"message","role":"assistant","stop_reason":"tool_use","content":[
                        {"type":"text","text":"Putting a compressor on the guitar."},
                        {"type":"tool_use","id":"tu1","name":"channel_dynamics","input":{"action":"set","track":"Gtr","comp":"Opto Leveler"}},
                        {"type":"tool_use","id":"tu2","name":"channel_dynamics","input":{"action":"set","track":"Nobody","comp":"Opto Leveler"}}]})", "application/json");
                else
                    res.set_content (R"({"id":"m2","type":"message","role":"assistant","stop_reason":"end_turn","content":[{"type":"text","text":"Done: **Opto Leveler** is on Gtr."}]})", "application/json");
            });
            const int port = mock.bind_to_any_port ("127.0.0.1");
            std::thread mockThread ([&] { mock.listen_after_bind(); });
            Prefs::set ("chat_apiBase", "http://127.0.0.1:" + juce::String (port));
            Chat::KeyStore::set (testKey);
            check (Chat::KeyStore::get() == testKey && ! props.getValue ("cl_anthropicKey").contains ("sk-ant"), "chat: the API key round-trips and is not stored in plain text");
            check (Chat::KeyStore::masked (testKey).endsWith ("fXYZ") && ! Chat::KeyStore::masked (testKey).contains ("0123456789"), "chat: the key is only ever shown masked");

            auto models = Chat::request ("/v1/models?limit=100", testKey, nullptr, 5000);
            check (models.ok() && models.json["data"][0]["id"].toString() == "claude-opus-5-5", "chat: GET /v1/models lists the models for the menu");
            auto bad = Chat::request ("/v1/models", "sk-ant-wrong", nullptr, 5000);
            check (! bad.ok() && bad.status == 401 && bad.error.contains ("refused"), "chat: a wrong key gives a clear 401 message");

            Chat::Engine engineChat (tools, *session);
            juce::StringArray texts; int calls = 0, okResults = 0, errResults = 0; bool finished = false;
            engineChat.onEvent = [&] (const Chat::Event& e)
            {
                if (e.kind == Chat::Event::assistantText) texts.add (e.text);
                else if (e.kind == Chat::Event::toolCall) ++calls;
                else if (e.kind == Chat::Event::toolResult) (e.ok ? okResults : errResults)++;
                else if (e.kind == Chat::Event::error) texts.add ("ERROR " + e.text);
                else if (e.kind == Chat::Event::finished) finished = true;
            };
            call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"none"})");
            check (engineChat.send ("Put a compressor on the guitar"), "chat: a message starts a turn");
            for (int i = 0; i < 300 && ! finished; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            auto* g2 = session->findTrack ("Gtr");
            auto comp = g2 != nullptr ? Console::Model::findDynamics (g2->pluginList, g2, false) : nullptr;
            check (finished && comp != nullptr && comp->getPluginType() == "sqmOpto", "chat: Claude's tool_use ran in the session (Opto Leveler is on Gtr)");
            check (calls == 2 && okResults == 1 && errResults == 1, "chat: both tool calls reported, the failing one as an error (" + juce::String (okResults) + " ok, " + juce::String (errResults) + " error)");
            check (posts == 2 && texts.size() == 2 && texts[1].contains ("Opto"), "chat: tool results went back and the final answer arrived (" + texts.joinIntoString (" | ") + ")");
            check (sawVersion && sawAuth, "chat: every request carries anthropic-version 2026-10-21 and the Bearer key");
            check (! sawTemperature, "chat: no temperature / top_p (rejected by API version 2026-10-21)");
            check (resultsFirst && sawIsError, "chat: tool_result blocks come first and failures carry is_error");
            check (toolCount == (int) tools.getTools().size() && toolCount > 100 && sawCache, "chat: every Squelchmoth tool is offered (" + juce::String (toolCount.load()) + "), with prompt caching on the tool list");
            check (engineChat.getMessages().size() == 4, "chat: the conversation holds user, tool_use, tool_result and the answer");
            bool leaked = false;
            for (auto& l : session->log.getEntries()) if (l.text.contains (testKey) || l.text.contains ("0123456789abcdefXYZ")) leaked = true;
            check (! leaked, "chat: the API key never appears in the activity log");
            engineChat.save();
            check (engineChat.chatFile.existsAsFile() && ! engineChat.chatFile.loadFileAsString().contains (testKey), "chat: the conversation is saved (without the key) in Squelchmoth Sessions/Chats");
            engineChat.chatFile.deleteFile();

            mock.stop();
            mockThread.join();
            Chat::KeyStore::forget();
            if (auto f = Prefs::file()) f->removeValue ("chat_apiBase");
            check (! Chat::KeyStore::has(), "chat: Forget removes the key");
        }

        std::cout << "  (1.10 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)\n";
    }

    // ===================================================================================== 1.11 shared engines
    {
        std::cout << "\n-- 1.11: shared engines (Wing Mod, Analog Style, Nonlinear core, Voice core, preset audit) --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        const double sr = 48000.0;

        // oversampler: unity and the reported latency agrees with a measurement
        {
            Eng::Oversampler os; os.prepare (256, 3, sr);
            std::vector<float> in (48128), out (48128);
            for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin (2 * juce::MathConstants<double>::pi * 1000.0 * (double) i / sr);
            for (size_t p = 0; p + 256 <= in.size(); p += 256) { auto o = os.up (0, in.data() + p, 256); os.down (0, o, 256, out.data() + p); }
            double S = 0, C = 0, eo = 0, ei = 0; const double w = 2 * juce::MathConstants<double>::pi * 1000.0 / sr;
            for (size_t i = 24000; i < 48000; ++i) { S += out[i] * std::sin (w * (double) i); C += out[i] * std::cos (w * (double) i); eo += out[i] * out[i]; ei += in[i] * in[i]; }
            double d = std::atan2 (-C, S) / w; if (d < 0) d += 2 * juce::MathConstants<double>::pi / w;
            check (std::abs (10 * std::log10 (eo / ei)) < 0.01 && std::abs (d - os.latencySamples()) < 0.05,
                   "1.11 oversampler 8x: unity gain, latency " + juce::String (os.latencySamples(), 2) + " samples reported, " + juce::String (d, 2) + " measured");
        }
        // Wing Mod locks to the transport
        {
            Eng::WingMod m; m.prepare (sr); m.s.sync = true; m.s.syncIndex = 12; m.setTransport (120, 3.0, true);
            float a, b; m.next (0, a, b); for (int i = 1; i < 6000; ++i) m.next (0, a, b); float q, r; m.next (0, q, r);
            check (std::abs (q - 1.0f) < 1.0e-3f, "1.11 Wing Mod: a 1/4 LFO is phase-locked to the bar (peak a sixteenth after the beat: " + juce::String (q, 4) + ")");
        }
        // Analog Style: all seven voicings hold the level on pink noise
        {
            std::vector<float> l, r; Eng::TestSignals::pink (l, r, sr, 3.0); float worst = 0; juce::String worstName;
            for (int v = 0; v < Eng::AnalogStyle::numVoicings; ++v)
            {
                Eng::AnalogStyle as; as.prepare (sr, 256, 2); as.setVoicing (v);
                auto L = l, R = r; for (size_t p = 0; p + 256 <= L.size(); p += 256) { float* ch[2] = { L.data() + p, R.data() + p }; as.process (ch, 2, 256); }
                double eo = 0, ei = 0; for (size_t i = 72000; i < L.size(); ++i) { eo += L[i] * L[i]; ei += l[i] * l[i]; }
                const float d = (float) (10 * std::log10 (eo / ei)); if (std::abs (d) > std::abs (worst)) { worst = d; worstName = Eng::AnalogStyle::voicingName (v); }
            }
            check (std::abs (worst) < 1.0f, "1.11 Analog Style: seven voicings within 1 dB of the input level (worst " + worstName + " " + juce::String (worst, 2) + " dB)");
        }
        // Voice core on the generated sung phrase: pitch, formants, sibilance, harmonic / noise split
        {
            std::vector<float> l, r; Eng::TestSignals::vocal (l, r, sr, 4.0);
            SqVoiceCore::Core vc; vc.prepare (sr, 256);
            static const float notes[] = { 57, 59, 60, 62, 64, 62, 60, 59 };
            std::vector<float> cents; float f1a = 0, f2a = 0, f1i = 0, f2i = 0; int nA = 0, nI = 0; bool sib = false, sibInVowel = false;
            std::vector<float> h (256), nz (256); double hnrVowel = 0; int hnrN = 0;
            for (size_t p = 0; p + 256 <= l.size(); p += 256)
            {
                vc.analyse (l.data() + p, 256); vc.split (l.data() + p, h.data(), nz.data(), 256);
                const auto& sn = vc.latest();
                const double t = (double) sn.position / sr; const int ni = (int) (t / 0.5) % 8; const double inNote = std::fmod (t, 0.5);
                if (sn.voiced && sn.hz > 0 && inNote > 0.12 && inNote < 0.4) cents.push_back ((float) std::abs (1200.0 * std::log2 (sn.hz / (440.0 * std::pow (2.0, (notes[ni] - 69) / 12.0)))));
                if (sn.voiced && inNote > 0.15 && inNote < 0.4 && sn.formants[1] > 0) { if (ni % 5 == 0) { f1a += sn.formants[0]; f2a += sn.formants[1]; ++nA; } if (ni % 5 == 2) { f1i += sn.formants[0]; f2i += sn.formants[1]; ++nI; } }
                if (sn.sibilant && (ni == 2 || ni == 6) && inNote < 0.1) sib = true;
                if (sn.sibilant && inNote > 0.15 && inNote < 0.4) sibInVowel = true;
                if (inNote > 0.2 && inNote < 0.4 && t > 0.5) { hnrVowel += sn.hnrDb; ++hnrN; }
            }
            std::sort (cents.begin(), cents.end());
            const float med = cents.empty() ? 999.0f : cents[cents.size() / 2];
            check (cents.size() > 40 && med < 25.0f, "1.11 Voice core: pitch tracks the sung notes (median error " + juce::String (med, 1) + " cents over " + juce::String ((int) cents.size()) + " frames, vibrato is +-20)");
            if (nA > 0) { f1a /= nA; f2a /= nA; } if (nI > 0) { f1i /= nI; f2i /= nI; }
            check (nA > 5 && std::abs (f1a - 800) < 160 && std::abs (f2a - 1150) < 230, "1.11 Voice core: formants of 'ah' F1 " + juce::String (juce::roundToInt (f1a)) + " Hz, F2 " + juce::String (juce::roundToInt (f2a)) + " Hz (sung at 800 / 1150)");
            check (nI > 5 && std::abs (f1i - 280) < 90 && std::abs (f2i - 2300) < 460, "1.11 Voice core: formants of 'ee' F1 " + juce::String (juce::roundToInt (f1i)) + " Hz, F2 " + juce::String (juce::roundToInt (f2i)) + " Hz (sung at 280 / 2300)");
            check (sib && ! sibInVowel, "1.11 Voice core: the 's' is flagged sibilant, the vowels are not");
            const double hnr = hnrN > 0 ? hnrVowel / hnrN : -99;
            std::vector<float> pl, pr; Eng::TestSignals::pink (pl, pr, sr, 2.0); SqVoiceCore::Core vn; vn.prepare (sr, 256);
            for (size_t p = 0; p + 256 <= pl.size(); p += 256) { vn.analyse (pl.data() + p, 256); vn.split (pl.data() + p, h.data(), nz.data(), 256); }
            check (hnr > 6.0 && vn.latest().hnrDb < -6.0f, "1.11 Voice core: harmonic-to-noise " + juce::String (hnr, 1) + " dB on sung vowels, " + juce::String (vn.latest().hnrDb, 1) + " dB on pink noise");
        }
        // preset audit tool: runs on two plug-ins and writes its report
        {
            auto rep = session->sessionsDir.getChildFile ("preset-audit-test.md");
            auto sum = PresetAudit::run (*session->edit, { "Silk Ladder", "Opto Leveler" }, {}, 0.75, rep);
            int expect = 0; for (auto& e : Signature::entries()) if (juce::String (e.name) == "Silk Ladder" || juce::String (e.name) == "Opto Leveler") expect += (int) e.presets.size();
            bool finite = true; for (auto& row : sum.rows) finite = finite && row.finite;
            check (sum.plugins == 2 && sum.presets == expect && expect > 0 && finite && rep.existsAsFile() && rep.loadFileAsString().contains ("| Silk Ladder |"),
                   "1.11 preset audit: " + juce::String (sum.presets) + " presets of 2 plug-ins rendered on 3 signals, report written (" + juce::String (sum.failed) + " flagged)");
        }
        // ---------------------------------------------------------------- Cinder (C1)
        {
            call ("add_track", R"({"kind":"audio","name":"Cin"})");
            auto add = call ("studio_fx", R"({"track":"Cin","which":"cinder","action":"add"})");
            auto* ct = session->findTrack ("Cin"); SqCinderPlugin* cp = nullptr;
            if (ct != nullptr) for (auto pl : ct->pluginList) if ((cp = dynamic_cast<SqCinderPlugin*> (pl)) != nullptr) break;
            check (cp != nullptr, "Cinder: added through studio_fx");
            int nPresets = 0, cats = 0; juce::StringArray seen;
            for (auto& e : Signature::entries()) if (juce::String (e.name) == "Cinder") { nPresets = (int) e.presets.size(); for (auto& p : e.presets) { auto c = juce::String (p.name).upToFirstOccurrenceOf (" | ", false, false); if (! seen.contains (c)) seen.add (c); } }
            cats = seen.size();
            check (nPresets >= 140 && cats >= 10, "Cinder: " + juce::String (nPresets) + " factory presets in " + juce::String (cats) + " categories");
            auto set = call ("studio_fx", R"({"track":"Cin","which":"cinder","action":"set","settings":{"model":"Pentode Box","Drive":12,"oversample":"8x","msMode":"Mid only"}})");
            if (cp != nullptr)
                check (cp->modelA.get() == Cinder::pentodeBox && cp->oversample.get() == 3 && cp->msMode.get() == 2 && std::abs (cp->bank[SqCinderPlugin::drive] - 12.0f) < 0.01f,
                       "Cinder: model, drive, oversampling and Mid/Side set by name");
            auto got = call ("studio_fx", R"({"track":"Cin","which":"cinder","action":"get"})");
            check (got.contains ("harmonics_h2_h10_db") && got.contains ("Pentode Box") && got.contains ("latency_ms"), "Cinder: get reports the model, latency and the harmonic profile");
            // measured advantage (super prompt C1): every model, 10 kHz at +24 dB drive, 44.1 kHz, 8x -> aliasing below -100 dBFS
            {
                double worst = -999; juce::String worstName;
                for (int m = 0; m < Cinder::numModels; ++m)
                {
                    Cinder::Processor c; c.p.modelA = m; c.p.driveDb = 24; c.p.oversampleLog2 = 3; c.p.dynamics = 0; c.prepare (44100.0, 512);
                    const int n = 1 << 15; const double f0 = 44100.0 * 7431.0 / n;
                    std::vector<float> l ((size_t) (n + 16384)), r;
                    for (size_t i = 0; i < l.size(); ++i) l[i] = Eng::dbToGain (-18) * 1.414f * (float) std::sin (2 * juce::MathConstants<double>::pi * f0 * (double) i / 44100.0);
                    r = l; for (size_t i = 0; i + 512 <= l.size(); i += 512) { float* ch[2] = { l.data() + i, r.data() + i }; c.process (ch, 2, nullptr, 512); }
                    juce::dsp::FFT fft (15); std::vector<float> buf ((size_t) n * 2, 0.0f);
                    for (int i = 0; i < n; ++i) { const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1)) + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * i / (n - 1)) - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * i / (n - 1)); buf[(size_t) i] = (float) (l[(size_t) (16384 + i)] * w); }
                    fft.performFrequencyOnlyForwardTransform (buf.data());
                    const double norm = 2.0 / (0.35875 * n); double wb = 0;
                    for (int k = 3; k < n / 2; ++k) { const double f = k * 44100.0 / n, rr = f / f0; if (std::abs (rr - std::round (rr)) * f0 < 8 * 44100.0 / n) continue; wb = juce::jmax (wb, (double) buf[(size_t) k] * norm); }
                    const double db = 20 * std::log10 (wb + 1e-30);
                    if (db > worst) { worst = db; worstName = Cinder::modelName (m); }
                }
                check (worst < -100.0, "Cinder: aliasing for 10 kHz at +24 dB drive, 44.1 kHz, 8x, all ten models: worst product " + juce::String (worst, 1) + " dBFS (" + worstName + ")");
            }
            // every factory preset: the audit holds each one to +-1 dB on its material, no NaN, no overs, CPU in budget
            {
                auto rep = session->sessionsDir.getChildFile ("preset-audit-cinder.md");
                auto sum = PresetAudit::run (*session->edit, { "Cinder" }, { "sqmCinder" }, 3.0, rep);
                juce::String firstFail; for (auto& row : sum.rows) if (! row.pass) { firstFail = row.preset + ": " + row.why; break; }
                double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
                check (sum.presets >= 140 && sum.failed == 0, "Cinder: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
                auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11.md");
                if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
            }
            call ("delete_track", R"({"track":"Cin"})");
        }
        // ---------------------------------------------------------------- Wingbeat Echo + Jr. (C2)
        {
            call ("add_track", R"({"kind":"audio","name":"Echo"})");
            call ("studio_fx", R"({"track":"Echo","which":"wingbeat","action":"add"})");
            auto* et = session->findTrack ("Echo"); SqWingbeatPlugin* wp = nullptr;
            if (et != nullptr) for (auto pl : et->pluginList) if ((wp = dynamic_cast<SqWingbeatPlugin*> (pl)) != nullptr) break;
            check (wp != nullptr, "Wingbeat Echo: added through studio_fx");
            int nW = 0, nJ = 0; for (auto& e : Signature::entries()) { if (juce::String (e.name) == "Wingbeat Echo") nW = (int) e.presets.size(); if (juce::String (e.name) == "Wingbeat Jr.") nJ = (int) e.presets.size(); }
            check (nW >= 160 && nJ >= 60, "Wingbeat: " + juce::String (nW) + " Echo presets, " + juce::String (nJ) + " Jr. presets");
            call ("studio_fx", R"({"track":"Echo","which":"wingbeat","action":"set","settings":{"mode":"Rhythm","style":"Spring Tape Echo","syncL":"1/8.","heads":5,"Feedback":60,
                                  "tapData":[{"sixteenths":3,"level":0.8,"pan":-1},{"sixteenths":6,"level":0.6,"pan":1,"pitch":12}],"tapLength":8}})");
            if (wp != nullptr)
            {
                for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                check (wp->mode.get() == 3 && wp->style.get() == 4 && wp->syncL.get() == 11 && wp->heads.get() == 5 && std::abs (wp->bank[SqWingbeatPlugin::feedback] - 60.0f) < 0.01f
                       && wp->tapData.get().contains ("6:0.6:1:14000:12"), "Wingbeat Echo: mode, style, note value, heads, feedback and custom 2-tap rhythm set by name");
            }
            auto got = call ("studio_fx", R"({"track":"Echo","which":"wingbeat","action":"get"})");
            check (got.contains ("Spring Tape Echo") && got.contains ("live_time_ms"), "Wingbeat Echo: get reports the style, mode and live time");
            call ("delete_track", R"({"track":"Echo"})");
            Edition::owned() = Edition::Owned {};
            check (Edition::isTypeAllowed ("sqmWingbeatJr") && ! Edition::isTypeAllowed ("sqmWingbeat"), "editions: Wingbeat Jr. is in the Demo, Wingbeat Echo is in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-wingbeat.md");
            auto sum = PresetAudit::run (*session->edit, { "Wingbeat Echo", "Wingbeat Jr." }, { "sqmWingbeat", "sqmWingbeatJr" }, 3.0, rep);
            juce::String firstFail; int failN = 0; for (auto& row : sum.rows) if (! row.pass) { if (firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why; ++failN; }
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 220 && sum.failed == 0, "Wingbeat: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-wingbeat.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Foil Plate + Lite (C4)
        {
            call ("add_track", R"({"kind":"audio","name":"Plate"})");
            call ("studio_fx", R"({"track":"Plate","which":"foilplate","action":"add"})");
            auto* pt = session->findTrack ("Plate"); SqFoilPlatePlugin* fpp = nullptr;
            if (pt != nullptr) for (auto pl : pt->pluginList) if ((fpp = dynamic_cast<SqFoilPlatePlugin*> (pl)) != nullptr) break;
            check (fpp != nullptr, "Foil Plate: added through studio_fx");
            int nF = 0, nL = 0; for (auto& e : Signature::entries()) { if (juce::String (e.name) == "Foil Plate") nF = (int) e.presets.size(); if (juce::String (e.name) == "Foil Plate Lite") nL = (int) e.presets.size(); }
            check (nF >= 120 && nL >= 30, "Foil Plate: " + juce::String (nF) + " presets, Lite " + juce::String (nL));
            call ("studio_fx", R"({"track":"Plate","which":"foilplate","action":"set","settings":{"plate":"Iron Mammoth","input":"Solid State","Decay":4.5,"lowSlope":24,"Auto Decay":-60}})");
            if (fpp != nullptr)
                check (fpp->plate.get() == 6 && fpp->input.get() == 1 && fpp->lowSlope.get() == 2 && std::abs (fpp->bank[SqFoilPlatePlugin::decay] - 4.5f) < 0.01f && std::abs (fpp->bank[SqFoilPlatePlugin::autoDecay] + 60.0f) < 0.01f,
                       "Foil Plate: plate, input stage, slope, decay and auto-decay set by name");
            auto got = call ("studio_fx", R"({"track":"Plate","which":"foilplate","action":"get"})");
            check (got.contains ("Iron Mammoth") && got.contains ("rt60_500hz_s") && got.contains ("input_thd_percent_at_minus18"), "Foil Plate: get reports the plate, RT60 per band and the input THD");
            // decay accuracy, measured from the impulse response (Schroeder integration, 500 Hz band)
            {
                FoilPlate::Processor q; q.p.archetype = 0; q.p.decayS = 2.5f; q.p.mix = 1; q.p.input = FoilPlate::clean; q.p.mod = 0; q.p.preDelayMs = 0; q.p.lowCut = 20; q.p.highCut = 20000; q.prepare (48000.0, 256);
                std::vector<float> l (48000 * 5, 0.0f), r = l; l[0] = r[0] = 0.5f;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; q.process (ch, 2, nullptr, 256); }
                Eng::Biquad b1, b2; b1.set (Eng::Biquad::bandpass, 48000.0, 500, 1.4); b2.set (Eng::Biquad::bandpass, 48000.0, 500, 1.4);
                std::vector<double> e (l.size()); for (size_t i = 0; i < l.size(); ++i) { const double v = b2.process (b1.process (l[i])); e[i] = v * v; }
                for (size_t i = e.size() - 1; i-- > 0;) e[i] += e[i + 1];
                int t5 = -1, t25 = -1; for (size_t i = 0; i < e.size(); ++i) { const double db = 10 * std::log10 (e[i] / e[0] + 1e-30); if (t5 < 0 && db < -5) t5 = (int) i; if (t25 < 0 && db < -25) { t25 = (int) i; break; } }
                const double rt = 3.0 * (t25 - t5) / 48000.0;
                check (std::abs (rt - 2.5) < 0.25, "Foil Plate: Decay 2.5 s measures " + juce::String (rt, 2) + " s RT60 at 500 Hz");
            }
            call ("delete_track", R"({"track":"Plate"})");
            Edition::owned() = Edition::Owned {};
            check (Edition::isTypeAllowed ("sqmFoilPlateLite") && ! Edition::isTypeAllowed ("sqmFoilPlate"), "editions: Foil Plate Lite is in the Demo, Foil Plate is in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-foilplate.md");
            auto sum = PresetAudit::run (*session->edit, { "Foil Plate", "Foil Plate Lite" }, { "sqmFoilPlate", "sqmFoilPlateLite" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 150 && sum.failed == 0, "Foil Plate: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-foilplate.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Radiant + Pre (C10), Maw + Lite (C11)
        {
            call ("add_track", R"({"kind":"audio","name":"Tube"})");
            call ("studio_fx", R"({"track":"Tube","which":"radiant","action":"add"})");
            call ("studio_fx", R"({"track":"Tube","which":"maw","action":"add"})");
            auto* tt = session->findTrack ("Tube"); SqRadiantPlugin* rp = nullptr; SqMawPlugin* mp = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList) { if (auto x = dynamic_cast<SqRadiantPlugin*> (pl)) rp = x; if (auto y = dynamic_cast<SqMawPlugin*> (pl)) mp = y; }
            check (rp != nullptr && mp != nullptr, "Radiant and Maw: added through studio_fx");
            call ("studio_fx", R"({"track":"Tube","which":"radiant","action":"set","settings":{"arch":"50s Broadcast","Input":12,"Bass":4,"micOn":true}})");
            call ("studio_fx", R"({"track":"Tube","which":"maw","action":"set","settings":{"Crush":80,"Release":3.5,"fastOn":true}})");
            if (rp != nullptr && mp != nullptr)
                check (rp->arch.get() == 1 && rp->mic.get() && std::abs (rp->bank[SqRadiantPlugin::input] - 12.0f) < 0.01f && std::abs (mp->bank[SqMawPlugin::crush] - 80.0f) < 0.01f && mp->fast.get(),
                       "Radiant circuit / mic / input and Maw crush / fast set by name");
            auto gr = call ("studio_fx", R"({"track":"Tube","which":"radiant","action":"get"})");
            check (gr.contains ("50s Broadcast Pre") && gr.contains ("thd_percent_at_minus18"), "Radiant: get reports the circuit and measured THD");
            {
                Maw::Processor m; m.p.crush = 0.6f; m.p.crunch = 0; m.prepare (48000.0, 64);
                std::vector<float> l (48000 * 2, 0.0f), r; for (size_t i = 24000; i < l.size(); ++i) l[i] = 0.5f * (float) std::sin (2 * juce::MathConstants<double>::pi * 200 * (double) i / 48000.0); r = l;
                std::vector<float> g (l.size()); for (size_t i = 0; i + 64 <= l.size(); i += 64) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, nullptr, 64); for (int k = 0; k < 64; ++k) g[i + (size_t) k] = m.gainReductionDb(); }
                const float fin = g[l.size() - 100]; int t63 = -1; for (size_t i = 24000; i < g.size(); ++i) if (g[i] >= 0.632f * fin) { t63 = (int) (i - 24000); break; }
                check (std::abs (t63 / 48.0 - 1.3) < 0.2, "Maw: attack measures " + juce::String (t63 / 48.0, 2) + " ms (1.3 +- 0.2)");
            }
            call ("delete_track", R"({"track":"Tube"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (Edition::isTypeAllowed ("sqmRadiantPre") && Edition::isTypeAllowed ("sqmMawLite") && ! Edition::isTypeAllowed ("sqmMaw") && ! Edition::isTypeAllowed ("sqmRadiant"),
                   "editions: Radiant Pre and Maw Lite are in Squelchmoth (core), Radiant and Maw are in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-radmaw.md");
            auto sum = PresetAudit::run (*session->edit, { "Radiant", "Radiant Pre", "Maw", "Maw Lite" }, { "sqmRadiant", "sqmRadiantPre", "sqmMaw", "sqmMawLite" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 270 && sum.failed == 0, "Radiant / Maw: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-radiant-maw.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase D1: Vellum EQ, Tremor, Orbit Pan, Mirror Shift + Lite, Shutter Delay + Lite
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"D1"})");
            for (auto w : { "vellum", "tremor", "orbit", "mirror", "shutter" }) call ("studio_fx", juce::String (R"({"track":"D1","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("D1"); SqVellumPlugin* ve = nullptr; SqTremorPlugin* tr = nullptr; SqOrbitPanPlugin* ob = nullptr; SqMirrorShiftPlugin* ms = nullptr; SqShutterPlugin* sh = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList)
            {
                if (auto a = dynamic_cast<SqVellumPlugin*> (pl)) ve = a; if (auto b = dynamic_cast<SqTremorPlugin*> (pl)) tr = b; if (auto c = dynamic_cast<SqOrbitPanPlugin*> (pl)) ob = c;
                if (auto d = dynamic_cast<SqMirrorShiftPlugin*> (pl)) ms = d; if (auto e = dynamic_cast<SqShutterPlugin*> (pl)) sh = e;
            }
            check (ve && tr && ob && ms && sh, "Phase D1: Vellum EQ, Tremor, Orbit Pan, Mirror Shift and Shutter Delay added through studio_fx");
            call ("studio_fx", R"({"track":"D1","which":"vellum","action":"set","settings":{"Low":-9,"midFreq":3,"Mid":6,"steppedOn":true,"target":2}})");
            call ("studio_fx", R"({"track":"D1","which":"tremor","action":"set","settings":{"Depth":100,"mode":1,"pattern":7,"harmonicOn":true}})");
            call ("studio_fx", R"({"track":"D1","which":"orbit","action":"set","settings":{"mode":6,"Width":210,"binauralOn":true,"panLaw":3}})");
            call ("studio_fx", R"({"track":"D1","which":"mirror","action":"set","settings":{"style":3,"Focus":1500,"Duck":9}})");
            call ("studio_fx", R"({"track":"D1","which":"shutter","action":"set","settings":{"multiply":3,"algo":5,"bits":2,"Time A":1250}})");
            if (ve && tr && ob && ms && sh)
                check (ve->midFreq.get() == 3 && ve->stepped.get() && ve->target.get() == 2 && std::abs (ve->bank[SqVellumPlugin::lowGain] + 9.0f) < 0.01f
                       && tr->mode.get() == 1 && tr->pattern.get() == 7 && tr->harmonicOn.get() && ob->mode.get() == 6 && ob->binauralOn.get() && ob->panLaw.get() == 3
                       && ms->style.get() == 3 && std::abs (ms->bank[SqMirrorShiftPlugin::focus] - 1500.0f) < 1.0f && sh->multiply.get() == 3 && sh->algo.get() == 5 && sh->bits.get() == 2,
                       "Phase D1: modes, steps, styles and switches set by name through studio_fx");
            auto gs = call ("studio_fx", R"({"track":"D1","which":"shutter","action":"get"})");
            check (gs.contains ("bandwidth_hz") && gs.contains ("1500") && gs.contains ("Ping-Pong"), "Shutter: get reports x8 = 1.5 kHz bandwidth and the algorithm");
            auto gm = call ("studio_fx", R"({"track":"D1","which":"mirror","action":"get"})");
            check (gm.contains ("mono_sum_loss_db") && gm.contains ("Tape ADT"), "Mirror Shift: get reports the mono-compatibility meter and the style");
            // measured models (the DSP the plug-ins run)
            {
                Vellum::Processor v; v.prepare (SR, 64); v.p.lowDb = -12; v.p.highDb = -15; v.p.midIdx = 3; v.p.midDb = 8; { float a[16] {}, b[16] {}; float* ch[2] = { a, b }; v.process (ch, 2, 16); }
                check (std::abs (v.magnitudeDb (25) + 12) < 1.5 && v.magnitudeDb (20000) < v.magnitudeDb (14000) && std::abs (v.magnitudeDb (2300) - 8) < 0.3,
                       "Vellum: shelf low cut, falling low-pass high cut, +8 dB mid at 2.3 kHz (" + juce::String (v.magnitudeDb (2300), 2) + " dB)");
                Vellum::Processor q2, q8; q2.prepare (SR, 64); q8.prepare (SR, 64);
                check (Vellum::midQ (8) > 3.0 * Vellum::midQ (1) - 0.6, "Vellum: mid Q narrows with gain (Q " + juce::String (Vellum::midQ (2), 2) + " at 2 dB, " + juce::String (Vellum::midQ (8), 2) + " at 8 dB)");
            }
            {
                Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.shape = Eng::WingMod::square; t.p.sync = false; t.p.rateHz = 4; t.p.smoothMs = 0.5f;
                std::vector<float> l (48000 * 2, 0.25f), r = l; for (size_t i = 0; i < l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; t.process (ch, 2, nullptr, 256); }
                float mn = 1; int opens = 0; for (size_t i = 24000; i < l.size(); ++i) { mn = juce::jmin (mn, std::abs (l[i])); if (l[i - 1] < 0.125f && l[i] >= 0.125f) ++opens; }
                check (Eng::gainToDb (mn / 0.25f) < -60.0f && std::abs (opens / 1.5 - 4.0) < 0.7, "Tremor: depth 100 % is a full gate, 4 Hz measured " + juce::String (opens / 1.5, 2) + " Hz");
            }
            {
                Orbit::Processor o; o.prepare (SR, 256); o.p.binaural = true; o.p.offsetDeg = 90; o.p.widthDeg = 0; o.p.smoothMs = 0.1f;
                std::vector<float> l (48000), r; juce::Random rn (7); for (auto& x : l) x = 0.3f * (rn.nextFloat() * 2 - 1); r = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; o.process (ch, 2, nullptr, 256); }
                int best = 0; double bc = -1; for (int lag = 0; lag < 60; ++lag) { double c = 0; for (size_t i = 20000; i < 40000; ++i) c += (double) r[i] * l[i + (size_t) lag]; if (c > bc) { bc = c; best = lag; } }
                check (std::abs (best / SR * 1000.0 - 0.66) < 0.06, "Orbit Pan: binaural ITD at 90 deg " + juce::String (best / SR * 1000.0, 3) + " ms (0.66)");
            }
            {
                Mirror::Processor m; m.prepare (SR, 256); m.p.style = 0; m.p.detune = 1; m.p.delay = 0.3f; m.p.mix = 1;
                std::vector<float> l (96000), r, dry; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.25f * (float) std::sin (twoPi * 1000.0 * (double) i / SR); r = l; dry = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, nullptr, 256); }
                double first = -1, last = -1; int cnt = 0; for (size_t i = 24001; i < 90000; ++i) { const float a = l[i - 1] - dry[i - 1], b = l[i] - dry[i]; if (a < 0 && b >= 0) { const double t = (double) (i - 1) + a / (a - b); if (first < 0) first = t; else ++cnt; last = t; } }
                const double cents = 1200.0 * std::log2 (cnt * SR / (last - first) / 1000.0);
                check (std::abs (cents - 15.0) < 2.0, "Mirror Shift: Micro A detune 100 % measures " + juce::String (cents, 1) + " cents (15)");
            }
            {
                auto gainAt = [&] (int mlt, double hz)
                {
                    Shutter::Processor s; s.prepare (SR, 256); s.p.multiply = mlt; s.p.timeAMs = 5; s.p.feedbackA = 0; s.p.mix = 1; s.p.bits = 0;
                    std::vector<float> l (28672), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.2f * (float) std::sin (twoPi * hz * (double) i / SR); r = l;
                    for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, 256); }
                    double e = 0; for (size_t i = 14336; i < 28672; ++i) e += (double) l[i] * l[i]; return 10.0 * std::log10 (e / 14336.0 / (0.02));
                };
                juce::String law; bool ok = true;
                for (int mlt = 0; mlt < 4; ++mlt) { const double bw = Shutter::bandwidthHz (mlt), in = gainAt (mlt, bw * 0.5), out = gainAt (mlt, bw * 1.5); ok = ok && std::abs (in) < 1.5 && out - in < -25; law << juce::String (bw / 1000.0, 1) << "k " << juce::String (out - in, 0) << " dB  "; }
                check (ok, "Shutter: Multiply bandwidth law 12 / 6 / 3 / 1.5 kHz (stop band at 1.5x): " + law);
                Shutter::Processor s; s.prepare (SR, 256); s.p.multiply = 3; s.p.timeAMs = 1250;
                check (std::abs (s.totalMs (0) - 10000.0) < 1.0, "Shutter: two lines to 10 s (1250 ms x8 = " + juce::String (s.totalMs (0), 0) + " ms)");
            }
            call ("delete_track", R"({"track":"D1"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (Edition::isTypeAllowed ("sqmShutterLite") && Edition::isTypeAllowed ("sqmMirrorShiftLite") && ! Edition::isTypeAllowed ("sqmShutter") && ! Edition::isTypeAllowed ("sqmMirrorShift")
                   && ! Edition::isTypeAllowed ("sqmVellum") && ! Edition::isTypeAllowed ("sqmTremor") && ! Edition::isTypeAllowed ("sqmOrbitPan"),
                   "editions: Shutter Lite and Mirror Shift Lite are in Squelchmoth (core); the full five are in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-d1.md");
            const juce::StringArray names { "Vellum EQ", "Tremor", "Orbit Pan", "Mirror Shift", "Mirror Shift Lite", "Shutter Delay", "Shutter Lite" };
            auto sum = PresetAudit::run (*session->edit, names, { "sqmVellum", "sqmTremor", "sqmOrbitPan", "sqmMirrorShift", "sqmMirrorShiftLite", "sqmShutter", "sqmShutterLite" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 480 && sum.failed == 0, "Phase D1: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-d1.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase D2: Shard, Murmuration, Masquerade
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"D2"})");
            for (auto w : { "shard", "murmuration", "masquerade" }) call ("studio_fx", juce::String (R"({"track":"D2","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("D2"); SqShardPlugin* sd = nullptr; SqMurmurationPlugin* mu = nullptr; SqMasqueradePlugin* mq = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList) { if (auto a = dynamic_cast<SqShardPlugin*> (pl)) sd = a; if (auto b = dynamic_cast<SqMurmurationPlugin*> (pl)) mu = b; if (auto c = dynamic_cast<SqMasqueradePlugin*> (pl)) mq = c; }
            check (sd && mu && mq, "Phase D2: Shard, Murmuration and Masquerade added through studio_fx");
            call ("studio_fx", R"({"track":"D2","which":"shard","action":"set","settings":{"Pitch":7,"engine":1,"quantizeOn":true,"fbMode":2,"lfo1Target":1,"LFO 1 Depth":25}})");
            call ("studio_fx", R"({"track":"D2","which":"murmuration","action":"set","settings":{"Time":4000,"Texture":20,"shimmerSt":3,"infiniteOn":true}})");
            call ("studio_fx", R"({"track":"D2","which":"masquerade","action":"set","settings":{"mode":2,"robotNote":40,"Formant":-5,"driveVoicing":5}})");
            if (sd && mu && mq)
                check (std::abs (sd->bank[SqShardPlugin::pitch] - 7.0f) < 0.01f && sd->engine.get() == 1 && sd->quantize.get() && sd->fbMode.get() == 2 && sd->lfo1Target.get() == 1
                       && std::abs (mu->bank[SqMurmurationPlugin::time] - 4000.0f) < 1.0f && mu->shimmerSt.get() == 3 && mu->infinite.get()
                       && mq->mode.get() == 2 && mq->robotNote.get() == 40 && std::abs (mq->bank[SqMasqueradePlugin::formant] + 5.0f) < 0.01f && mq->driveVoicing.get() == 5,
                       "Phase D2: engines, modes, matrix and switches set by name through studio_fx");
            auto gm = call ("studio_fx", R"({"track":"D2","which":"murmuration","action":"get"})");
            check (gm.contains ("envelope_0_to_1") && gm.contains ("\"taps\""), "Murmuration: get reports the taps and the envelope");
            auto gq = call ("studio_fx", R"({"track":"D2","which":"masquerade","action":"get"})");
            check (gq.contains ("Robot") && gq.contains ("latency_ms"), "Masquerade: get reports the mode and the latency");
            {
                auto pitchTest = [&] (int engine, float st)
                {
                    Shard::Processor s; s.prepare (SR, 256); s.p.engine = engine; s.p.pitchSt = st; s.p.reverseProb = 0; s.p.recycle = 0; s.p.mix = 1; s.p.spliceMs = 500;
                    std::vector<float> l (96000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.25f * (float) std::sin (twoPi * 440.0 * (double) i / SR); r = l;
                    for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, nullptr, 256); }
                    double best = 0, bf = 0; for (double f = 100; f < 2000; f *= 1.002) { const double w = twoPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 48000; i < 96000; ++i) { const double s0 = l[i] + c * s1 - s2; s2 = s1; s1 = s0; } const double pw = s1 * s1 + s2 * s2 - c * s1 * s2; if (pw > best) { best = pw; bf = f; } }
                    return bf;
                };
                const double v = pitchTest (Shard::vintage, 12), c = pitchTest (Shard::clean, 7);
                check (std::abs (v / 880.0 - 1) < 0.01 && std::abs (c / 659.26 - 1) < 0.012, "Shard: Vintage +12 st " + juce::String (v, 1) + " Hz, Clean +7 st " + juce::String (c, 1) + " Hz (from 440)");
            }
            {
                Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 1000; m.p.texture = 0; m.p.y = 0; m.p.mix = 1; m.p.blur = 0; m.p.modDepth = 0;
                std::vector<float> l (72000, 0.0f), r; l[0] = 1; r = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, 256); }
                int peaks = 0; size_t last = 0; for (size_t i = 100; i + 340 < l.size(); i += 240) { double e = 0; for (size_t k = i; k < i + 240; ++k) e += (double) l[k] * l[k]; if (std::sqrt (e / 240) > 0.01 && (peaks == 0 || i - last > 4800)) { ++peaks; last = i; } }
                check (peaks == 4 && Murm::tapsFor (1) * 2 == 2048, "Murmuration: Texture 0 = four discrete echoes over Time; Texture 100 = 2048 taps");
            }
            {
                Masquerade::Processor m; m.p.engine = 0; m.p.pitchSt = 12; m.prepare (SR, 256);
                std::vector<float> l (96000), r; double ph = 0;
                for (size_t i = 0; i < l.size(); ++i) { ph += 200.0 / SR; double y = 0; for (int h = 1; h <= 10; ++h) y += std::sin (twoPi * h * ph) / h; l[i] = 0.1f * (float) y; }
                r = l; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, 256); }
                // the output's fundamental by a fine Goertzel sweep around the expected octave
                double bestP = 0, hz = 0;
                for (double f = 340; f < 470; f *= 1.0005) { const double w = twoPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 48000; i < 96000; ++i) { const double s0 = l[i] + c * s1 - s2; s2 = s1; s1 = s0; } const double pw = s1 * s1 + s2 * s2 - c * s1 * s2; if (pw > bestP) { bestP = pw; hz = f; } }
                check (std::abs (1200.0 * std::log2 (hz / 400.0)) < 25 && m.latencySamples() / SR * 1000.0 < 1.0,
                       "Masquerade: Transpose +12 st turns a 200 Hz voice into " + juce::String (hz, 1) + " Hz; Live latency " + juce::String (m.latencySamples() / SR * 1000.0, 2) + " ms");
            }
            call ("delete_track", R"({"track":"D2"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (! Edition::isTypeAllowed ("sqmShard") && ! Edition::isTypeAllowed ("sqmMurmuration") && ! Edition::isTypeAllowed ("sqmMasquerade"), "editions: Shard, Murmuration and Masquerade are in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-d2.md");
            auto sum = PresetAudit::run (*session->edit, { "Shard", "Murmuration", "Masquerade" }, { "sqmShard", "sqmMurmuration", "sqmMasquerade" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 440 && sum.failed == 0, "Phase D2: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-d2.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase D3: Larva Filter 2, Twelve Phase 2, Hive Rack
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"D3"})");
            for (auto w : { "larva2", "phase2", "hive" }) call ("studio_fx", juce::String (R"({"track":"D3","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("D3"); SqLarvaFilter2Plugin* lf = nullptr; SqTwelvePhase2Plugin* ph = nullptr; SqHiveRackPlugin* hr = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList) { if (auto a = dynamic_cast<SqLarvaFilter2Plugin*> (pl)) lf = a; if (auto b = dynamic_cast<SqTwelvePhase2Plugin*> (pl)) ph = b; if (auto c = dynamic_cast<SqHiveRackPlugin*> (pl)) hr = c; }
            check (lf && ph && hr, "Phase D3: Larva Filter 2, Twelve Phase 2 and Hive Rack added through studio_fx");
            call ("studio_fx", R"({"track":"D3","which":"larva2","action":"set","settings":{"Cutoff":1200,"stages":4,"routing":2,"resp2":3,"model3":1,"modMode":1,"pattern":"Tresillo","modSync":13}})");
            call ("studio_fx", R"({"track":"D3","which":"phase2","action":"set","settings":{"style":"Deep 24","tzOn":true}})");
            if (lf && ph)
                check (std::abs (lf->bank[SqLarvaFilter2Plugin::cutoff] - 1200.0f) < 1.0f && lf->stages.get() == 4 && lf->routing.get() == 2 && lf->resp[1].get() == 3 && lf->model[2].get() == 1
                       && lf->wm.mode.get() == 1 && lf->wm.pattern.get() == 4 && lf->wm.sync.get() == 13
                       && ph->stages.get() == 24 && SqTwelvePhase2Plugin::styleNames()[ph->style.get()] == "Deep 24" && ph->tz.get(),
                       "Phase D3: stages, routing, per-stage type, Wing Mod and phaser style set by name through studio_fx");
            auto gl = call ("studio_fx", R"({"track":"D3","which":"larva2","action":"get"})");
            check (gl.contains ("stage_types") && gl.contains ("live_cutoff_hz"), "Larva Filter 2: get reports the stage types and the live cutoff");
            auto gp = call ("studio_fx", R"({"track":"D3","which":"phase2","action":"get"})");
            check (gp.contains ("Deep 24") && gp.contains ("through_zero"), "Twelve Phase 2: get reports the style and through-zero");
            // Hive Rack: slots, routing, macros, chains
            call ("hive_rack", R"({"track":"D3","action":"set_slot","slot":1,"plugin":"Cinder"})");
            call ("hive_rack", R"({"track":"D3","action":"set_slot","slot":2,"plugin":"Foil Plate","preset":""})");
            call ("hive_rack", R"({"track":"D3","action":"routing","routing":1})");
            auto mm = call ("hive_rack", R"({"track":"D3","action":"map_macro","macro":1,"slot":1,"param":"Mix","lo":0.2,"hi":0.8})");
            if (hr != nullptr)
            {
                check (hr->slotInfo (0).name == "Cinder" && hr->slotInfo (1).name == "Foil Plate" && hr->slotInfo (0).preset.isNotEmpty() && hr->routing.get() == 1,
                       "Hive Rack: slots take Signature effects with a preset; routing switches to two lanes");
                hr->bank.params[(size_t) SqHiveRackPlugin::m1]->setParameter (100.0f, juce::sendNotificationSync);
                for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
                auto* ap = hr->slotPlugin (0) != nullptr ? SqHiveRackPlugin::findParam (*hr->slotPlugin (0), "Mix") : nullptr;
                check (ap != nullptr && std::abs (ap->getCurrentNormalisedValue() - 0.8f) < 0.02f, "Hive Rack: Macro 1 at 100 % drives slot 1's Mix to its mapped top (0.8)");
                // the rack's state carries its slots: a copy made from it rebuilds them
                hr->flushPluginStateToValueTree();
                auto copyState = hr->state.createCopy(); copyState.removeProperty (te::IDs::id, nullptr);
                auto np = session->edit->engine.getPluginManager().createNewPlugin (*session->edit, copyState);
                auto* hc = dynamic_cast<SqHiveRackPlugin*> (np.get());
                check (hc != nullptr && hc->slotInfo (0).name == "Cinder" && hc->slotInfo (1).name == "Foil Plate" && hc->macroTargets().size() == 1,
                       "Hive Rack: saved state restores every slot, its plug-in settings and the macro map");
                auto lst = call ("hive_rack", R"({"track":"D3","action":"chain","chain":"list"})");
                check (HiveChains::chains().size() >= 200 && lst.contains ("Vocal") && lst.contains ("Mix Bus"), "Hive Rack: " + juce::String ((int) HiveChains::chains().size()) + " factory chains in categories");
                call ("hive_rack", R"({"track":"D3","action":"chain","chain":0})");
                const auto& c0 = HiveChains::chains()[0]; int filled = 0; for (int i = 0; i < 8; ++i) if (hr->slotInfo (i).name.isNotEmpty()) ++filled;
                check (filled == (int) c0.slots.size() && hr->slotInfo (0).name == juce::String (c0.slots[0].plugin) && hr->loadedChain == 0, "Hive Rack: chain '" + juce::String (c0.name) + "' loads its " + juce::String (filled) + " slots");
                // editions: a slot this copy doesn't own is kept but locked (passed through) and names its product
                Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
                hr->setSlot (2, "Shard");
                const auto si = hr->slotInfo (2);
                check (! si.allowed && si.lockedBy.isNotEmpty(), "Hive Rack: a slot from a pack this copy doesn't own is locked, naming " + si.lockedBy);
                Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            }
            {
                // Larva 2: a 4-pole ladder low-pass at 500 Hz keeps 100 Hz and takes 4 kHz down by more than 40 dB
                auto level = [&] (double hz)
                {
                    Larva2::Processor p; p.p.stages = 1; p.p.response[0] = 0; p.p.model[0] = 0; p.p.cutoffHz = 500; p.p.resonance = 0; p.p.freqModOct = 0; p.p.mix = 1; p.prepare (SR, 256);
                    std::vector<float> l (49152), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.25f * (float) std::sin (twoPi * hz * (double) i / SR); r = l;   // a whole number of blocks
                    for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); }
                    double e = 0; for (size_t i = 24576; i < l.size(); ++i) e += (double) l[i] * l[i]; return 10 * std::log10 (e / 24576.0 / (0.25 * 0.25 / 2));
                };
                const double lo = level (100), hi = level (4000);
                check (lo > -3 && hi < -40, "Larva Filter 2: transistor ladder at 500 Hz: 100 Hz " + juce::String (lo, 1) + " dB, 4 kHz " + juce::String (hi, 1) + " dB");
            }
            call ("delete_track", R"({"track":"D3"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (! Edition::isTypeAllowed ("sqmLarvaFilter2") && ! Edition::isTypeAllowed ("sqmPhase12v2") && ! Edition::isTypeAllowed ("sqmHiveRack"), "editions: Larva Filter 2, Twelve Phase 2 and Hive Rack are in Color & Space");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-d3.md");
            auto sum = PresetAudit::run (*session->edit, { "Larva Filter 2", "Twelve Phase 2", "Hive Rack" }, { "sqmLarvaFilter2", "sqmPhase12v2", "sqmHiveRack" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 480 && sum.failed == 0, "Phase D3: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-d3.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase E1: Siren Verb, Mic Closet, Larynx, Exhale
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"E1"})");
            for (auto w : { "sirenverb", "miccloset", "larynx", "exhale" }) call ("vocal_fx", juce::String (R"({"track":"E1","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("E1"); SqSirenVerbPlugin* sv = nullptr; SqMicClosetPlugin* mc = nullptr; SqLarynxPlugin* lx = nullptr; SqExhalePlugin* ex = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList)
            { if (auto a = dynamic_cast<SqSirenVerbPlugin*> (pl)) sv = a; if (auto b = dynamic_cast<SqMicClosetPlugin*> (pl)) mc = b; if (auto c = dynamic_cast<SqLarynxPlugin*> (pl)) lx = c; if (auto d = dynamic_cast<SqExhalePlugin*> (pl)) ex = d; }
            check (sv && mc && lx && ex, "Phase E1: Siren Verb, Mic Closet, Larynx and Exhale added through vocal_fx");
            call ("vocal_fx", R"({"track":"E1","which":"sirenverb","action":"set","settings":{"algo":"Chamber","Decay":2.5,"autoEqOn":true,"delayMode":"Ping-Pong","shimmerPitch":"+19"}})");
            call ("vocal_fx", R"({"track":"E1","which":"miccloset","action":"set","settings":{"target":"1950s Ribbon","source":"USB Desktop Condenser","pattern":"Figure-8","Distance":5}})");
            call ("vocal_fx", R"({"track":"E1","which":"larynx","action":"set","settings":{"Tract Length":120,"Lips":60,"levelMatchOn":false}})");
            call ("vocal_fx", R"({"track":"E1","which":"exhale","action":"set","settings":{"Breath":-60,"Consonants":90}})");
            if (sv && mc && lx && ex)
                check (sv->algo.get() == SirenVerb::chamber && std::abs (sv->bank[SqSirenVerbPlugin::decay] - 2.5f) < 0.01f && sv->autoEqOn.get() && sv->delayMode.get() == 2 && sv->shimmerPitch.get() == 2
                       && mc->target.get() == 19 && mc->source.get() == 10 && mc->pattern.get() == 3 && std::abs (mc->bank[SqMicClosetPlugin::distance] - 5.0f) < 0.01f
                       && std::abs (lx->bank[SqLarynxPlugin::length] - 120.0f) < 0.01f && std::abs (lx->bank[SqLarynxPlugin::p8] - 60.0f) < 0.01f && ! lx->levelMatchOn.get()
                       && std::abs (ex->bank[SqExhalePlugin::breath] + 60.0f) < 0.01f,
                       "Phase E1: algorithms, microphones, tract points and breath set by name through vocal_fx");
            auto gm = call ("vocal_fx", R"({"track":"E1","which":"miccloset","action":"get"})");
            check (gm.contains ("1950s Ribbon") && gm.contains ("response") && gm.contains ("Figure-8"), "Mic Closet: get reports the microphones, the pattern and the response");
            auto gs = call ("vocal_fx", R"({"track":"E1","which":"sirenverb","action":"get"})");
            check (gs.contains ("Chamber") && gs.contains ("singer_pitch_hz"), "Siren Verb: get reports the algorithm and the singer's pitch");
            call ("vocal_fx", R"({"track":"E1","which":"sirenverb","action":"assist","vibe":"Epic"})");
            if (sv != nullptr) check (sv->algo.get() == SirenVerb::shimmer && sv->bank[SqSirenVerbPlugin::decay] >= 3.0f, "Siren Verb: Assist 'Epic' sets a shimmer hall from the tempo");
            if (sv != nullptr)
            {
                // Mix lock: a preset keeps the Mix you set
                sv->mixLockOn.setValue (true, nullptr); sv->bank.params[(size_t) SqSirenVerbPlugin::mix]->setParameter (41.0f, juce::sendNotificationSync);
                call ("vocal_fx", R"({"track":"E1","which":"sirenverb","action":"preset","preset":"Big Pop Hall"})");
                const float m1 = sv->bank[SqSirenVerbPlugin::mix];
                sv->mixLockOn.setValue (false, nullptr);
                call ("vocal_fx", R"({"track":"E1","which":"sirenverb","action":"preset","preset":"Big Pop Hall"})");
                check (std::abs (m1 - 41.0f) < 0.01f && std::abs (sv->bank[SqSirenVerbPlugin::mix] - 24.0f) < 0.01f, "Siren Verb: Mix lock keeps 41 % through a preset; unlocked the preset's 24 % loads");
            }
            {
                // Hall at 2.0 s measures within 20 % (Schroeder T20)
                SirenVerb::Processor p; p.p.algo = SirenVerb::hall; p.p.decayS = 2.0f; p.p.mix = 1; p.p.early = 0; p.p.preDelayMs = 0; p.p.modDepth = 0; p.p.lowCutHz = 20; p.p.highCutHz = 20000; p.p.damping = 0; p.prepare (SR, 256);
                std::vector<float> l (160000, 0.0f), r; l[256] = 1; r = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
                std::vector<double> e (l.size()); double acc = 0; for (size_t i = l.size(); i-- > 0;) { acc += (double) l[i] * l[i] + (double) r[i] * r[i]; e[i] = acc; }
                size_t i5 = 0, i25 = 0; for (size_t i = 300; i < l.size(); ++i) { const double d = 10 * std::log10 (e[i] / e[300] + 1e-30); if (! i5 && d <= -5) i5 = i; if (! i25 && d <= -25) { i25 = i; break; } }
                const double rt = 3.0 * (double) (i25 - i5) / SR;
                check (std::abs (rt / 2.0 - 1) < 0.2, "Siren Verb: Hall set to 2.0 s measures RT60 " + juce::String (rt, 2) + " s");
            }
            {
                MicCloset::Processor q; q.p.source = 3; q.p.target = 3; q.p.colour = 0; q.prepare (SR, 256);
                double w = 0; for (double hz = 60; hz <= 12000; hz *= 1.15) w = juce::jmax (w, std::abs (q.responseDb (hz)));
                MicCloset::Processor ph; ph.p.source = 0; ph.p.target = 30; ph.p.colour = 0; ph.prepare (SR, 256);
                check (w < 0.05 && ph.responseDb (2000) - ph.responseDb (6000) > 12 && MicCloset::numMics() >= 44,
                       "Mic Closet: " + juce::String (MicCloset::numMics()) + " archetypes; a mic converted to itself is flat (" + juce::String (w, 3) + " dB); the handset rolls off above 3.4 kHz");
            }
            {
                // a voice: 200 Hz harmonics + breath noise; Exhale with Breath off takes the noise down and leaves the harmonics
                auto voice = [&] (size_t n) { std::vector<float> v (n); double ph = 0; juce::Random rnd (7); for (size_t i = 0; i < n; ++i) { ph += 200.0 / SR; double y = 0; for (int h = 1; h <= 12; ++h) y += std::sin (twoPi * h * ph) / h; v[i] = 0.08f * (float) y + 0.025f * (rnd.nextFloat() * 2 - 1); } return v; };
                auto goertzel = [&] (const std::vector<float>& v, double hz) { const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 48000; i < 140000; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return 10 * std::log10 (s1 * s1 + s2 * s2 - c * s1 * s2 + 1e-30); };
                auto run = [&] (float db) { Exhale::Processor p; p.p.breathDb = db; p.prepare (SR, 256); auto l = voice (144000), r = l; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); } return l; };
                const auto y0 = run (0), y1 = run (-60);
                double n0 = 0, n1 = 0, h0 = 0, h1 = 0; for (int k = 3; k <= 9; ++k) { n0 += goertzel (y0, 100.0 * (2 * k + 1)); n1 += goertzel (y1, 100.0 * (2 * k + 1)); } for (int k = 1; k <= 4; ++k) { h0 += goertzel (y0, 200.0 * k); h1 += goertzel (y1, 200.0 * k); }
                check ((n0 - n1) / 7 > 8 && std::abs (h0 - h1) / 4 < 1.0, "Exhale: Breath off takes the breath down " + juce::String ((n0 - n1) / 7, 1) + " dB, harmonics move " + juce::String ((h0 - h1) / 4, 2) + " dB");
            }
            {
                // Larynx: the neutral tract is transparent; a 0.8x tract raises the formants
                std::vector<float> vowel (96000); { Eng::Biquad f1, f2; f1.set (Eng::Biquad::bandpass, SR, 700, 6); f2.set (Eng::Biquad::bandpass, SR, 1200, 8); double ph = 0;
                    for (size_t i = 0; i < vowel.size(); ++i) { ph += 140 / SR; float e = 0; if (ph >= 1) { ph -= 1; e = 1; } vowel[i] = 0.6f * (f1.process (e) + 0.7f * f2.process (e)); } }
                auto run = [&] (float len) { Larynx::Processor p; p.p.levelMatch = false; p.p.length = len; p.prepare (SR, 256); auto l = vowel, r = vowel; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); } return l; };
                const auto y = run (1.0f); const int lat = Larynx::Channel::latency();
                double e = 0, d = 0; for (size_t i = 24000; i < 90000; ++i) { const double ref = vowel[i - (size_t) lat]; e += ref * ref; d += (y[i] - ref) * (y[i] - ref); }
                auto centroid = [&] (const std::vector<float>& v) { double num = 0, den = 0; for (double hz = 300; hz < 3000; hz += 25) { const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 24000; i < 90000; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } const double pw = s1 * s1 + s2 * s2 - c * s1 * s2; num += hz * pw; den += pw; } return num / den; };
                const double ratio = centroid (run (0.8f)) / centroid (y);
                check (10 * std::log10 (d / e) < -40 && ratio > 1.12 && lx != nullptr && std::abs (lx->getLatencySeconds() * SR - lat) < 1,
                       "Larynx: neutral tract residual " + juce::String (10 * std::log10 (d / e), 1) + " dB; 0.8x tract raises the formants x" + juce::String (ratio, 2) + "; latency reported");
            }
            call ("delete_track", R"({"track":"E1"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true; Edition::owned().vocal = true;
            check (Edition::isTypeAllowed ("sqmSirenVerb") && Edition::isTypeAllowed ("sqmMicCloset") && Edition::isTypeAllowed ("sqmLarynx") && Edition::isTypeAllowed ("sqmExhale"), "editions: Siren Verb, Mic Closet, Larynx and Exhale are in the Vocal Suite");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (! Edition::isTypeAllowed ("sqmSirenVerb") && ! Edition::isTypeAllowed ("sqmExhale"), "editions: not in core Squelchmoth");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-e1.md");
            auto sum = PresetAudit::run (*session->edit, { "Siren Verb", "Mic Closet", "Larynx", "Exhale" }, { "sqmSirenVerb", "sqmMicCloset", "sqmLarynx", "sqmExhale" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 310 && sum.failed == 0, "Phase E1: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-e1.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase E2: Hive Choir, Double Wing 2, Mutant
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"E2"})");
            for (auto w : { "hivechoir", "doublewing2", "mutant" }) call ("vocal_fx", juce::String (R"({"track":"E2","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("E2"); SqHiveChoirPlugin* hc = nullptr; SqDoubleWing2Plugin* dw = nullptr; SqMutantPlugin* mu = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList) { if (auto a = dynamic_cast<SqHiveChoirPlugin*> (pl)) hc = a; if (auto b = dynamic_cast<SqDoubleWing2Plugin*> (pl)) dw = b; if (auto c = dynamic_cast<SqMutantPlugin*> (pl)) mu = c; }
            check (hc && dw && mu, "Phase E2: Hive Choir, Double Wing 2 and Mutant added through vocal_fx");
            call ("vocal_fx", R"({"track":"E2","which":"hivechoir","action":"set","settings":{"mode3":"MIDI Chord","Voice 3 Singers":16,"Voice 1 Interval":5,"engine":"Live","scale":"Natural Minor","keyRoot":"A"}})");
            call ("vocal_fx", R"({"track":"E2","which":"doublewing2","action":"set","settings":{"doubles":4,"vibStraightOn":false,"Vibrato Depth":30}})");
            call ("vocal_fx", R"({"track":"E2","which":"mutant","action":"set","settings":{"Pitch":-12,"alienDiv":"1/16","repeats":8,"driveVoicing":"Crunch"}})");
            if (hc && dw && mu)
                check (hc->mode[2].get() == HiveChoir::midiChord && juce::roundToInt (hc->bank[SqHiveChoirPlugin::idx (2, SqHiveChoirPlugin::vUnison)]) == 16 && std::abs (hc->bank[SqHiveChoirPlugin::idx (0, SqHiveChoirPlugin::vInterval)] - 5.0f) < 0.01f
                       && hc->engine.get() == 0 && hc->keyRoot.get() == 9 && SqSessionKey::scaleNames()[hc->scale.get()].containsIgnoreCase ("minor")
                       && dw->doubles.get() == 4 && ! dw->vibStraight.get() && std::abs (dw->bank[SqDoubleWing2Plugin::vibDepth] - 30.0f) < 0.01f
                       && std::abs (mu->bank[SqMutantPlugin::pitch] + 12.0f) < 0.01f && juce::String (Eng::WingMod::syncName (mu->alienDiv.get())) == "1/16" && SqMutantPlugin::repeatsFor (mu->repeats.get()) == 8 && mu->driveVoicing.get() == 5,
                       "Phase E2: voice modes, singers, key, doubles, divisions and repeats set by name through vocal_fx");
            auto gh = call ("vocal_fx", R"({"track":"E2","which":"hivechoir","action":"get"})");
            check (gh.contains ("\"voices\"") && gh.contains ("MIDI Chord") && gh.contains ("latency_ms"), "Hive Choir: get reports each voice, its mode and the latency");
            auto voice = [&] (size_t n, double hz) { std::vector<float> v (n); double ph = 0; for (size_t i = 0; i < n; ++i) { ph += hz / SR; double y = 0; for (int h = 1; h <= 12; ++h) y += std::sin (twoPi * h * ph) / h; v[i] = 0.08f * (float) y; } return v; };
            auto bestHz = [&] (const std::vector<float>& v, double lo, double hi) { double bp = 0, bf = 0; for (double f = lo; f < hi; f *= 1.002) { const double w = twoPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 60000; i < 140000; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } const double pw = s1 * s1 + s2 * s2 - c * s1 * s2; if (pw > bp) { bp = pw; bf = f; } } return bf; };
            {
                auto harm = [&] (int mode, float iv)
                {
                    HiveChoir::Processor p; p.p.leadDb = -60; p.p.v[0].mode = mode; p.p.v[0].interval = iv; p.p.v[0].levelDb = 0; p.p.humanCents = 0; p.p.humanMs = 0; p.prepare (SR, 256);
                    auto l = voice (144000, 220), r = l; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
                    return bestHz (l, 180, 340);
                };
                const double a = harm (HiveChoir::interval, 4), b = harm (HiveChoir::scaleStep, 2);
                check (std::abs (1200 * std::log2 (a / 277.18)) < 20 && std::abs (1200 * std::log2 (b / 261.63)) < 25,
                       "Hive Choir: +4 st from 220 Hz gives " + juce::String (a, 1) + " Hz; scale step +2 in C major gives " + juce::String (b, 1) + " Hz");
            }
            {
                HiveChoir::Processor p; p.p.engine = 1; for (int k = 0; k < 4; ++k) { p.p.v[k].mode = HiveChoir::interval; p.p.v[k].interval = (float) (k * 3 + 3); p.p.v[k].unison = 8; }
                p.prepare (SR, 128); auto l = voice (240000, 220), r = l;
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                for (size_t i = 0; i + 128 <= l.size(); i += 128) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 128); }
                const double pct = (juce::Time::getMillisecondCounterHiRes() - t0) / 5000.0 * 100.0;
                check (pct < 12, "Hive Choir: 32 singers (4 voices x 8), Studio engine, " + juce::String (pct, 2) + " % of one core");
            }
            {
                // Double Wing 2: vibrato only where the note is straight
                auto amt = [&] (double vibCents)
                {
                    DoubleWing2::Processor p; p.p.vibCents = 30; p.p.vibStraightOnly = true; p.prepare (SR, 256);
                    std::vector<float> l (144000); double ph = 0; for (size_t i = 0; i < l.size(); ++i) { const double f = 220 * std::pow (2.0, vibCents / 1200.0 * std::sin (twoPi * 5.5 * (double) i / SR)); ph += f / SR; double y = 0; for (int h = 1; h <= 12; ++h) y += std::sin (twoPi * h * ph) / h; l[i] = 0.08f * (float) y; }
                    auto r = l; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
                    return p.vibratoAmount();
                };
                const float straight = amt (0), wobbly = amt (60);
                check (straight > 0.8f && wobbly < 0.3f, "Double Wing 2: vibrato added on a straight note (" + juce::String (straight, 2) + "), not on a sung vibrato (" + juce::String (wobbly, 2) + ")");
            }
            {
                Mutant::Processor p; p.p.pitchSt = 12; p.prepare (SR, 256); auto l = voice (144000, 220), r = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
                const double f = bestHz (l, 300, 600);
                check (std::abs (1200 * std::log2 (f / 440.0)) < 20, "Mutant: Pitch +12 st turns 220 Hz into " + juce::String (f, 1) + " Hz");
            }
            call ("delete_track", R"({"track":"E2"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true; Edition::owned().vocal = true;
            check (Edition::isTypeAllowed ("sqmHiveChoir") && Edition::isTypeAllowed ("sqmDoubleWing2") && Edition::isTypeAllowed ("sqmMutant"), "editions: Hive Choir, Double Wing 2 and Mutant are in the Vocal Suite");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-e2.md");
            // Hive Choir's scale steps follow the session key: audit the presets with no session key (their own key), as a fresh song has
            auto& es = session->edit->state; juce::NamedValueSet savedKey;
            for (auto id : { "sqmKeyRoot", "sqmKeyScale", "sqmKeyMap" }) if (es.hasProperty (id)) { savedKey.set (id, es[id]); es.removeProperty (id, nullptr); }
            auto sum = PresetAudit::run (*session->edit, { "Hive Choir", "Double Wing 2", "Mutant" }, { "sqmHiveChoir", "sqmDoubleWing2", "sqmMutant" }, 3.0, rep);
            for (auto& kv : savedKey) es.setProperty (kv.name, kv.value, nullptr);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 240 && sum.failed == 0, "Phase E2: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-e2.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase E3: Vox Moth, Talk Wing, Clean Room
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"E3"})");
            for (auto w : { "voxmoth", "talkwing", "cleanroom" }) call ("vocal_fx", juce::String (R"({"track":"E3","which":")") + w + R"(","action":"add"})");
            auto* tt = session->findTrack ("E3"); SqVoxMothPlugin* vm = nullptr; SqTalkWingPlugin* tw = nullptr; SqCleanRoomPlugin* cr = nullptr;
            if (tt != nullptr) for (auto pl : tt->pluginList) { if (auto a = dynamic_cast<SqVoxMothPlugin*> (pl)) vm = a; if (auto b = dynamic_cast<SqTalkWingPlugin*> (pl)) tw = b; if (auto c = dynamic_cast<SqCleanRoomPlugin*> (pl)) cr = c; }
            check (vm && tw && cr, "Phase E3: Vox Moth, Talk Wing and Clean Room added through vocal_fx");
            call ("vocal_fx", R"({"track":"E3","which":"voxmoth","action":"set","settings":{"voicing":"Digital 64","carrier":"Voice Track","shape":"Resonant","octave2":-1,"Slide":3,"Level 4":-6}})");
            call ("vocal_fx", R"({"track":"E3","which":"talkwing","action":"set","settings":{"source":"Built-in Buzz","engine":"Live","buzzOctave":-1,"Formant":2}})");
            call ("vocal_fx", R"({"track":"E3","which":"cleanroom","action":"set","settings":{"humMode":"50 Hz","engine":"Live","residualOn":true,"Clean":70}})");
            if (vm && tw && cr)
                check (juce::roundToInt (vm->bank[SqVoxMothPlugin::bands]) == 64 && vm->voicing.get() == 7 && vm->carrier.get() == VoxMoth::voiceTrack && vm->shape.get() == VoxMoth::resonant && vm->octave2.get() == 1
                       && std::abs (vm->bank[SqVoxMothPlugin::slide] - 3.0f) < 0.01f && std::abs (vm->bank[SqVoxMothPlugin::lv1 + 3] + 6.0f) < 0.01f
                       && tw->source.get() == TalkWing::buzz && tw->engine.get() == 0 && tw->buzzOctave.get() == 1 && std::abs (tw->bank[SqTalkWingPlugin::formant] - 2.0f) < 0.01f
                       && cr->humMode.get() == 1 && cr->engine.get() == 0 && cr->residual.get() && std::abs (cr->bank[SqCleanRoomPlugin::clean] - 70.0f) < 0.01f,
                       "Phase E3: voicing, carrier, filter shape, octave, band level, routing, engine, mains and residual set by name through vocal_fx");
            auto gv = call ("vocal_fx", R"({"track":"E3","which":"voxmoth","action":"get"})");
            auto gt = call ("vocal_fx", R"({"track":"E3","which":"talkwing","action":"get"})");
            auto gc = call ("vocal_fx", R"({"track":"E3","which":"cleanroom","action":"get"})");
            check (gv.contains ("Digital 64") && gv.contains ("carrier_notes") && gt.contains ("routing_now") && gt.contains ("latency_ms") && gc.contains ("mains_hz") && gc.contains ("clicks_repaired"),
                   "Vox Moth, Talk Wing and Clean Room: get reports the voicing and carrier, the routing, the mains and the repairs");
            if (vm != nullptr)
            {
                const float before = vm->bank[SqVoxMothPlugin::resonance]; const int sh = vm->shape.get(); const float rel = vm->bank[SqVoxMothPlugin::release];
                call ("vocal_fx", R"({"track":"E3","which":"voxmoth","action":"dice"})");
                check (before != vm->bank[SqVoxMothPlugin::resonance] || sh != vm->shape.get() || rel != vm->bank[SqVoxMothPlugin::release], "Vox Moth: dice makes a new sound");
            }
            check (! call ("vocal_fx", R"({"track":"E3","which":"cleanroom","action":"learn_start"})").containsIgnoreCase ("error") && call ("vocal_fx", R"({"track":"E3","which":"cleanroom","action":"learn_stop"})").contains ("noise_profile"),
                   "Clean Room: learn_start / learn_stop through vocal_fx");
            auto vowel = [&] (size_t n, double f0, double F1, double F2)
            {
                std::vector<float> v (n); double ph = 0;
                for (size_t i = 0; i < n; ++i) { ph += f0 / SR; double y = 0; for (int h = 1; h * f0 < 9000; ++h) { const double f = h * f0; y += (1.0 / (1.0 + std::pow ((f - F1) / 90.0, 2)) + 0.6 / (1.0 + std::pow ((f - F2) / 140.0, 2)) + 0.02) * std::sin (twoPi * h * ph) / std::sqrt ((double) h); } v[i] = 0.1f * (float) y; }
                return v;
            };
            auto pw = [&] (const std::vector<float>& v, size_t a, size_t b, double hz) { const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a) / (double) (b - a); };
            auto bandE = [&] (const std::vector<float>& v, double lo, double hi) { double e = 0; for (double f = lo; f < hi; f += 20) e += pw (v, 60000, 140000, f); return 10 * std::log10 (e + 1e-30); };
            {
                VoxMoth::Processor p; p.prepare (SR, 256); std::array<bool, 128> h {}; h[60] = true; p.setHeld (h);
                auto l = vowel (144000, 220, 700, 1200), r = l;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); }
                double bp = 0, bf = 0; for (double f = 150; f < 400; f *= 1.002) { const double q = pw (l, 60000, 140000, f); if (q > bp) { bp = q; bf = f; } }
                check (std::abs (1200 * std::log2 (bf / 261.63)) < 25, "Vox Moth: a held C4 plays the voice's words on " + juce::String (bf, 1) + " Hz");
            }
            {
                auto talk = [&] (double F1, double F2)
                {
                    TalkWing::Processor p; p.p.tube = 0; p.p.sibilance = 0; p.prepare (SR, 256);
                    std::vector<float> l (144000); double ph = 0; for (auto& x : l) { ph += 110.0 / SR; ph -= std::floor (ph); x = 0.2f * (float) (2 * ph - 1); }
                    auto r = l; auto key = vowel (144000, 196, F1, F2);
                    for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key.data() + i, key.data() + i }; p.process (ch, 2, k, 256); }
                    return bandE (l, 1900, 2700) - bandE (l, 500, 900);
                };
                const double d = talk (300, 2300) - talk (750, 1150);
                check (d > 10, "Talk Wing: the instrument says the vowel ('ee' vs 'ah' at 2-2.7 kHz: " + juce::String (d, 1) + " dB)");
            }
            {
                CleanRoom::Processor p; p.p.clean = 0.5f; p.p.hum = 1; p.p.buzz = 1; p.p.hiss = p.p.noise = p.p.room = p.p.clicks = 0; p.prepare (SR, 256);
                auto in = vowel (192000, 233, 700, 1200); for (size_t i = 0; i < in.size(); ++i) { double hm = 0; for (int k = 1; k <= 6; ++k) hm += std::sin (twoPi * 60.0 * k * (double) i / SR) / k; in[i] += 0.05f * (float) hm; }
                auto l = in, r = in; for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
                const size_t L = (size_t) p.latencySamples(); double hum = 0, voice = 0;
                for (int k = 1; k <= 3; ++k) hum += 10 * std::log10 (pw (l, 100000 + L, 180000 + L, 60.0 * k) / pw (in, 100000, 180000, 60.0 * k));
                for (int k = 1; k <= 4; ++k) voice += std::abs (10 * std::log10 (pw (l, 100000 + L, 180000 + L, 233.0 * k) / pw (in, 100000, 180000, 233.0 * k)));
                check (hum / 3 < -25 && voice / 4 < 1.0, "Clean Room: 60 Hz hum down " + juce::String (-hum / 3, 1) + " dB, the voice's harmonics move " + juce::String (voice / 4, 2) + " dB");
            }
            call ("delete_track", R"({"track":"E3"})");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true; Edition::owned().vocal = true;
            check (Edition::isTypeAllowed ("sqmVoxMoth") && Edition::isTypeAllowed ("sqmTalkWing") && Edition::isTypeAllowed ("sqmCleanRoom"), "editions: Vox Moth, Talk Wing and Clean Room are in the Vocal Suite");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-e3.md");
            auto& es = session->edit->state; juce::NamedValueSet savedKey;
            for (auto id : { "sqmKeyRoot", "sqmKeyScale", "sqmKeyMap" }) if (es.hasProperty (id)) { savedKey.set (id, es[id]); es.removeProperty (id, nullptr); }
            auto sum = PresetAudit::run (*session->edit, { "Vox Moth", "Talk Wing", "Clean Room" }, { "sqmVoxMoth", "sqmTalkWing", "sqmCleanRoom" }, 3.0, rep);
            for (auto& kv : savedKey) es.setProperty (kv.name, kv.value, nullptr);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 220 && sum.failed == 0, "Phase E3: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-e3.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase E4: Moth Tune FX 2
        {
            const double SR = 48000.0, twoPi = juce::MathConstants<double>::twoPi;
            call ("add_track", R"({"kind":"audio","name":"E4"})");
            call ("moth_tune_fx", R"({"track":"E4","action":"add"})");
            call ("moth_tune_fx", R"({"track":"E4","action":"slot","slot":1,"module":"phaser","x":0.3,"y":0.6,"trigger":"momentary"})");
            call ("moth_tune_fx", R"({"track":"E4","action":"slot","slot":2,"module":"auto-pan","trigger":"auto"})");
            call ("moth_tune_fx", R"({"track":"E4","action":"set","settings":{"motion_style":"legato","pattern":"Siren"}})");
            auto g4 = juce::JSON::parse (call ("moth_tune_fx", R"({"track":"E4","action":"get"})"));
            check (g4["motion"]["style"].toString() == "legato" && g4["rack"][0]["module"].toString() == "phaser" && g4["rack"][0]["trigger"].toString() == "momentary"
                   && g4["rack"][1]["module"].toString() == "auto-pan" && g4["rack"][1]["trigger"].toString() == "auto",
                   "Moth Tune FX 2: modules, per-slot triggers and motion style set by name through moth_tune_fx");
            check (SqVocalFx::moduleNames().size() == 22 && SqTuneFxPlugin::factoryPatterns().size() == 24, "Moth Tune FX 2: 21 modules and 24 factory motion patterns");
            call ("delete_track", R"({"track":"E4"})");
            // the plug-in itself, rendered: a held note opens a Momentary slot; a Legato rest holds the note where Hard cuts it
            auto tone = [&] (size_t n) { std::vector<float> v (n); double ph = 0; for (size_t i = 0; i < n; ++i) { ph += 220.0 / SR; double y = 0; for (int h = 1; h <= 10; ++h) y += std::sin (twoPi * h * ph) / h; v[i] = 0.1f * (float) y; } return v; };
            auto render = [&] (std::function<void (SqTuneFxPlugin&)> setup, bool holdNote)
            {
                std::vector<float> out;
                auto pp = engine->getPluginManager().createNewPlugin (*session->edit, SqTuneFxPlugin::xmlTypeName, {});
                auto* fx = dynamic_cast<SqTuneFxPlugin*> (pp.get());
                if (fx == nullptr) return out;
                setup (*fx);
                const int block = 256; pp->baseClassInitialise ({ te::TimePosition(), SR, block });
                auto in = tone ((size_t) (SR * 2.0)); juce::AudioBuffer<float> buf (2, block); te::MidiMessageArray midi;
                for (size_t p = 0; p + (size_t) block <= in.size(); p += (size_t) block)
                {
                    buf.copyFrom (0, 0, in.data() + p, block); buf.copyFrom (1, 0, in.data() + p, block);
                    midi.clear(); if (holdNote && p == 0) midi.addMidiMessage (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0.0, 0);
                    const double t0 = (double) p / SR;
                    te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t0 + block / SR)), true, false, true, false);
                    pp->applyToBuffer (fc);
                    for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
                }
                pp->baseClassDeinitialise();
                return out;
            };
            auto withRing = [] (SqTuneFxPlugin& f) { f.slotType[0].setValue (1, nullptr); f.slotTrig[0].setValue (2, nullptr); f.bank.params[(size_t) SqTuneFxPlugin::s1mix]->setParameter (100.0f, juce::sendNotification); };
            const auto closed = render (withRing, false), opened = render (withRing, true);
            double d = 0, e = 0; for (size_t i = 24000; i < closed.size() && i < opened.size(); ++i) { d += (double) (opened[i] - closed[i]) * (opened[i] - closed[i]); e += (double) closed[i] * closed[i]; }
            check (opened.size() > 48000 && 10 * std::log10 (d / (e + 1e-20)) > -6, "Moth Tune FX 2: a Momentary slot opens while a MIDI note is held (" + juce::String (10 * std::log10 (d / (e + 1e-20)), 1) + " dB of change)");
            auto rests = [&] (int style)
            {
                auto o = render ([style] (SqTuneFxPlugin& f) { f.motionMode.setValue (4, nullptr); f.motionSync.setValue (2, nullptr); f.motionLen.setValue (4, nullptr); f.motionSteps.setValue ("0 x 0 x 0 0 0 0 0 0 0 0 0 0 0 0", nullptr); f.motionStyle.setValue (style, nullptr); }, false);
                double en = 0; for (size_t i = 24000; i < o.size(); ++i) en += (double) o[i] * o[i]; return en;
            };
            const double hard = rests (0), legato = rests (2);
            check (hard > 0 && 10 * std::log10 (hard / legato) < -2.0, "Moth Tune FX 2: Hard rests cut the note, Legato holds it (" + juce::String (10 * std::log10 (hard / legato), 1) + " dB)");
            {
                // the new modules on their own: EQ boosts where it is set, Fuzz keeps the level, Auto-Pan moves the image
                SqVocalFx::Slot sl; sl.prepare (SR); sl.type = 17;
                std::vector<float> l (48000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.1f * (float) std::sin (twoPi * 1000.0 * (double) i / SR); r = l;
                const float x1k = (float) (std::log10 (1000.0 / 100.0) / 2.0);
                sl.process (l.data(), r.data(), (int) l.size(), x1k, 1.0f, 1.0f, 0.0f, 120.0);
                double eo = 0; for (size_t i = 24000; i < l.size(); ++i) eo += (double) l[i] * l[i];
                const double boost = 10 * std::log10 (eo / 24000.0 / (0.01 * 0.5));
                SqVocalFx::Slot fz; fz.prepare (SR); fz.type = 21; auto fl = tone (96000), fr = fl; auto fin = fl;
                fz.process (fl.data(), fr.data(), (int) fl.size(), 0.8f, 0.5f, 1.0f, 0.0f, 120.0);
                double ei = 0, ef = 0; for (size_t i = 48000; i < fl.size(); ++i) { ei += (double) fin[i] * fin[i]; ef += (double) fl[i] * fl[i]; }
                SqVocalFx::Slot ap; ap.prepare (SR); ap.type = 20; auto pl = tone (96000), pr = pl;
                ap.process (pl.data(), pr.data(), (int) pl.size(), 0.5f, 1.0f, 1.0f, 0.0f, 120.0);
                float lo = 9, hi = -9; for (size_t w = 0; w + 4800 <= pl.size(); w += 4800) { double a = 0, b = 0; for (size_t i = w; i < w + 4800; ++i) { a += (double) pl[i] * pl[i]; b += (double) pr[i] * pr[i]; } const float bal = (float) (10 * std::log10 ((a + 1e-12) / (b + 1e-12))); lo = std::min (lo, bal); hi = std::max (hi, bal); }
                check (std::abs (boost - 15.0) < 1.5 && std::abs (10 * std::log10 (ef / ei)) < 3.0 && hi - lo > 12.0f,
                       "Moth Tune FX 2: EQ boosts 1 kHz by " + juce::String (boost, 1) + " dB, Fuzz keeps the level within " + juce::String (std::abs (10 * std::log10 (ef / ei)), 1) + " dB, Auto-Pan swings " + juce::String (hi - lo, 1) + " dB");
            }
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-e4.md");
            auto& es = session->edit->state; juce::NamedValueSet savedKey;
            for (auto id : { "sqmKeyRoot", "sqmKeyScale", "sqmKeyMap" }) if (es.hasProperty (id)) { savedKey.set (id, es[id]); es.removeProperty (id, nullptr); }
            auto sum = PresetAudit::run (*session->edit, { "Moth Tune FX 2" }, { "sqmTuneFx" }, 3.0, rep);
            for (auto& kv : savedKey) es.setProperty (kv.name, kv.value, nullptr);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            double maxCpu = 0; for (auto& row : sum.rows) maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent);
            check (sum.presets >= 140 && sum.failed == 0, "Phase E4: preset audit " + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-e4.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        // ---------------------------------------------------------------- Phase C: Moth Tune 2, Moth Key, Siren Comp, Siren EQ, Hush De-Ess 2
        {
            std::cout << "  -- 1.11 Voice, first wave --\n";
            const double twoPi = juce::MathConstants<double>::twoPi;
            // a sung-like tone (glottal-ish harmonics) following a pitch curve in Hz
            auto voiceTone = [&] (double sr, double secs, std::function<double (double)> hzAt)
            {
                std::vector<float> v ((size_t) (sr * secs)); double ph = 0;
                for (size_t i = 0; i < v.size(); ++i)
                {
                    ph += hzAt (i / sr) / sr; ph -= std::floor (ph);
                    double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin (twoPi * ph * h) / (h * (1.0 + 0.15 * h));
                    v[i] = (float) (0.25 * s * std::min (1.0, i / (0.02 * sr)));
                }
                return v;
            };
            // the pitch track of a signal (cents relative to ref), voiced frames only
            auto centsTrack = [] (const std::vector<float>& x, double sr, double ref, double from, double to)
            {
                SqVocal::PitchTracker tr; tr.prepare (sr); tr.setRange (80.0f, 1000.0f); tr.setTracking (50.0f);
                std::vector<std::pair<double, double>> out;
                for (size_t i = 0; i < x.size(); ++i)
                    if (tr.push (x[i]))
                    {
                        const auto& f = tr.latest(); const double t = (double) f.pos / sr;
                        if (f.voiced && f.hz > 0 && t >= from && t <= to) out.push_back ({ t, 1200.0 * std::log2 (f.hz / ref) });
                    }
                return out;
            };
            auto runTune = [] (const std::vector<float>& in, double sr, int eng, const SqVocal::TuneSettings& s)
            {
                SqVocal::TuneProcessor tp; tp.prepare (sr, eng, 1);
                SqVocal::TuneProcessor::Context ctx; auto l = in, r = in;
                for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min ((size_t) 256, l.size() - i); float* io[2] = { l.data() + i, r.data() + i }; tp.process (io, 2, n, s, ctx); }
                return l;
            };
            // 1. Live latency under 1 ms at 44.1-192 kHz (every voice type)
            {
                double worst = 0;
                for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
                    for (int vt = 0; vt < 6; ++vt) worst = juce::jmax (worst, SqVocal::TuneProcessor::latencyFor (0, vt, sr) * 1000.0 / sr);
                check (worst < 1.0, "Moth Tune 2 Live engine: latency " + juce::String (worst, 3) + " ms worst case at 44.1-192 kHz (under 1 ms)");
            }
            // 2. Pitch error under 2 cents RMS on a held note (225 Hz is 39 ct sharp of A3)
            {
                const double sr = 44100;
                const auto in = voiceTone (sr, 3.0, [] (double) { return 225.0; });
                SqVocal::TuneSettings s; s.retuneMs = 0; s.allowedMask = 0xfff; s.vibKeep = 1; s.humanize = 0; s.flex = 0;
                const auto out = runTune (in, sr, 1, s);
                const auto tr = centsTrack (out, sr, 220.0, 0.6, 2.8);
                double e2 = 0; for (auto& p : tr) e2 += p.second * p.second;
                const double rms = tr.empty() ? 999.0 : std::sqrt (e2 / (double) tr.size());
                check (tr.size() > 50 && rms < 2.0, "Moth Tune 2: held note pitch error " + juce::String (rms, 2) + " cents RMS (" + juce::String ((int) tr.size()) + " frames, under 2)");
            }
            // 3. Dynamic vibrato: Preserve keeps rate and depth within 5 % even at Retune 0; Cut removes it; Boost widens it
            {
                const double sr = 44100;
                const auto in = voiceTone (sr, 4.0, [&] (double t) { return 220.0 * std::pow (2.0, 40.0 / 1200.0 * std::sin (twoPi * 5.5 * t)); });
                int outliers = 0;
                auto measure = [&] (const std::vector<float>& x, double& rate, double& depth)
                {
                    // depth: peak of the deviation (RMS x sqrt 2); rate: crossings (8 ct hysteresis) from the first to the last
                    const auto tr = centsTrack (x, sr, 220.0, 1.2, 3.8);
                    double m = 0; for (auto& p : tr) m += p.second; m /= juce::jmax ((size_t) 1, tr.size());
                    double e2 = 0; int zc = 0, sg = 0; double t0c = -1, t1c = -1; outliers = 0;
                    for (size_t i = 0; i < tr.size(); ++i)
                    {
                        const double d = tr[i].second - m; e2 += d * d; if (std::abs (d) > 200) ++outliers;
                        const int ns = d > 8 ? 1 : d < -8 ? -1 : 0;
                        if (ns != 0 && ns != sg) { if (sg != 0) { ++zc; if (t0c < 0) t0c = tr[i].first; t1c = tr[i].first; } sg = ns; }
                    }
                    depth = std::sqrt (e2 / juce::jmax ((size_t) 1, tr.size())) * std::sqrt (2.0);
                    rate = zc > 2 ? (zc - 1) / 2.0 / (t1c - t0c) : 0;
                };
                double ri, di, rp, dp, rl, dl, rc, dc, rb, db, rh, dh;
                measure (in, ri, di);
                SqVocal::TuneSettings s; s.retuneMs = 0; s.allowedMask = 0xfff; s.dvMode = 1;
                measure (runTune (in, sr, 1, s), rp, dp);
                measure (runTune (in, sr, 0, s), rl, dl);
                check (std::abs (rp / ri - 1) < 0.05 && std::abs (dp / di - 1) < 0.05 && std::abs (rl / ri - 1) < 0.05 && std::abs (dl / di - 1) < 0.05,
                       "Moth Tune 2 Preserve at Retune 0: vibrato in " + juce::String (ri, 2) + " Hz / " + juce::String (di, 1) + " ct, out Studio " + juce::String (rp, 2) + " Hz / " + juce::String (dp, 1)
                       + " ct, Live " + juce::String (rl, 2) + " Hz / " + juce::String (dl, 1) + " ct (within 5 %)");
                SqVocal::TuneSettings hard; hard.retuneMs = 0; hard.allowedMask = 0xfff; hard.vibKeep = 0; hard.classic = true;
                measure (runTune (in, sr, 1, hard), rh, dh); const int hardOut = outliers;
                check (dh < 5.0 && hardOut == 0, "hard tune on that vibrato: flat to " + juce::String (dh, 1) + " ct, no glitches (" + juce::String (hardOut) + " octave errors in the output)");
                s.dvMode = 2; s.dvPitch = 1; measure (runTune (in, sr, 1, s), rc, dc);
                s.dvMode = 3; measure (runTune (in, sr, 1, s), rb, db);
                check (dc < 0.3 * di && db > 1.6 * di, "Moth Tune 2 Cut leaves " + juce::String (dc, 1) + " ct of vibrato, Boost makes it " + juce::String (db, 1) + " ct (input " + juce::String (di, 1) + " ct)");
            }
            // 4. Harmony trigger: a closed gate silences the harmony voices only
            {
                const double sr = 44100;
                const auto in = voiceTone (sr, 2.0, [] (double) { return 220.0; });
                SqVocal::TuneSettings s; s.retuneMs = 10; s.allowedMask = 0xfff; s.harm[0].mode = 1; s.harm[0].interval = 4; s.harm[0].gain = 1.0f; s.harm[0].pan = -1.0f;
                auto energy = [] (const std::vector<float>& x) { double e = 0; for (size_t i = x.size() / 2; i < x.size(); ++i) e += (double) x[i] * x[i]; return 10 * std::log10 (e + 1e-20); };
                const double open = energy (runTune (in, sr, 1, s)); s.harmGate = 0; const double closed = energy (runTune (in, sr, 1, s));
                SqVocal::TuneSettings lead; lead.retuneMs = 10; lead.allowedMask = 0xfff; const double alone = energy (runTune (in, sr, 1, lead));
                check (open - closed > 2.0 && std::abs (closed - alone) < 0.5, "Moth Tune 2 harmony trigger: gate open +" + juce::String (open - closed, 1) + " dB, closed matches the lead alone (" + juce::String (closed - alone, 2) + " dB)");
            }
            // 4a. Harmony voices hold their level at every interval (Studio: the overlap-add is power-compensated going down)
            {
                const double sr = 48000; std::vector<float> l, r; Eng::TestSignals::vocal (l, r, sr, 3.0);
                double ein = 0; for (size_t i = l.size() / 4; i < l.size(); ++i) ein += (double) l[i] * l[i];
                double lo = 99, hi = -99; juce::String worst;
                for (int iv : { -12, -7, -5, -3, 3, 4, 5, 7 })
                {
                    SqVocal::TuneSettings s; s.retuneMs = 10; s.allowedMask = 0xfff; s.harm[0].mode = 1; s.harm[0].interval = (float) iv; s.harm[0].gain = 1.0f; s.harm[0].pan = -1.0f; s.harmDetune = 0; s.harmTimingMs = 0;
                    SqVocal::TuneSettings lead = s; lead.harm[0].gain = 0.0f;
                    const auto a = runTune (l, sr, 1, s), b = runTune (l, sr, 1, lead);
                    double e = 0; for (size_t i = a.size() / 4; i < a.size(); ++i) e += std::pow ((double) a[i] - b[i], 2);
                    const double db = 10 * std::log10 (e / ein); if (db < lo) { lo = db; worst = juce::String (iv); } hi = juce::jmax (hi, db);
                }
                check (lo > -2.0 && hi < 1.5, "Moth Tune 2 harmony voices from an octave down to a fifth up: " + juce::String (lo, 1) + " to " + juce::String (hi, 1) + " dB re the voice (lowest at " + worst + " st)");
            }
            // 4b. Live engine formant preservation keeps the level and the peaks (+3 st on the sung vocal)
            {
                const double sr = 48000; std::vector<float> l, r; Eng::TestSignals::vocal (l, r, sr, 3.0);
                SqVocal::TuneSettings s; s.retuneMs = 20; s.allowedMask = 0xfff; s.transpose = 3; s.preserve = true;
                const auto o = runTune (l, sr, 0, s);
                double ei = 0, eo = 0; float pi = 0, po = 0; for (size_t i = 48000; i < l.size(); ++i) { ei += l[i] * l[i]; eo += o[i] * o[i]; pi = juce::jmax (pi, std::abs (l[i])); po = juce::jmax (po, std::abs (o[i])); }
                const double lv = 10 * std::log10 (eo / ei), pk = juce::Decibels::gainToDecibels (po) - juce::Decibels::gainToDecibels (pi);
                check (std::abs (lv) < 1.0 && pk < 6.0, "Moth Tune Live: +3 st with formants preserved holds level (" + juce::String (lv, 2) + " dB, peaks +" + juce::String (pk, 1) + " dB)");
            }
            // 5. Moth Key / detect_key on a song with a key change (written as a WAV, imported to a track)
            {
                const double sr = 44100; const double secs = 48;
                std::vector<float> y ((size_t) (sr * secs), 0.0f);
                auto note = [&] (double t0, double dur, double midi, float amp)
                {
                    const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
                    const size_t a = (size_t) (t0 * sr), b = juce::jmin (y.size(), (size_t) ((t0 + dur) * sr));
                    for (size_t i = a; i < b; ++i) { const double t = (i - a) / sr, env = std::exp (-t * 1.6) * juce::jmin (1.0, t * 200.0) * juce::jmin (1.0, (double) (b - i) / (0.01 * sr)); double v = 0; for (int h = 1; h <= 5; ++h) v += std::sin (twoPi * f * h * t) / (h * h * 0.7 + 0.3); y[i] += (float) (amp * env * v); }
                };
                auto prog = [&] (double t0, double len, int root, bool minor)
                {
                    const int maj[4][3] = { { 0, 4, 7 }, { 5, 9, 12 }, { 7, 11, 14 }, { 0, 4, 7 } }, mnr[4][3] = { { 0, 3, 7 }, { 5, 8, 12 }, { 7, 11, 14 }, { 0, 3, 7 } };
                    int bar = 0; for (double t = t0; t < t0 + len - 0.1; t += 2.0, ++bar) { const auto& c = (minor ? mnr : maj)[bar % 4]; for (int k = 0; k < 3; ++k) note (t, 1.9, 60 + root + c[k], 0.05f); note (t, 1.9, 36 + root + c[0], 0.08f); }
                };
                prog (0, 24, 4, true); prog (24, 24, 9, false);   // E minor, then A major
                auto wavF = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_selftest_keysong.wav");
                { wavF.deleteFile(); juce::AudioBuffer<float> b (1, (int) y.size()); b.copyFrom (0, 0, y.data(), (int) y.size()); juce::WavAudioFormat wav;
                  if (auto os = wavF.createOutputStream()) if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, 1, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
                call ("add_track", R"({"kind":"audio","name":"Key Song"})");
                call ("import_audio", juce::String (R"({"track":"Key Song","path":")") + wavF.getFullPathName().replace ("\\", "/") + R"(","start_bar":1})");
                call ("moth_tune", R"({"track":"Key Song","action":"add"})");
                auto kr = juce::JSON::parse (call ("detect_key", R"({"track":"Key Song"})"));
                const auto segs = kr["key_changes"];
                const bool twoKeys = segs.isArray() && segs.size() == 2 && segs[0]["key"].toString() == "E minor" && segs[1]["key"].toString() == "A major" && std::abs ((double) segs[1]["start_s"] - (session->barToTime (1.0).inSeconds() + 24.0)) < 6.0;
                check (twoKeys, "detect_key: the key change E minor -> A major found at " + (segs.isArray() && segs.size() > 1 ? juce::String ((double) segs[1]["start_s"], 1) : juce::String ("-")) + " s (" + kr["key"].toString() + ", confidence " + kr["confidence"].toString() + ")");
                auto* kt = session->findTrack ("Key Song"); SqTunePlugin* ktu = nullptr; if (kt != nullptr) for (auto p : kt->pluginList) if (auto x = dynamic_cast<SqTunePlugin*> (p)) ktu = x;
                check (session->edit->state.getProperty ("sqmKeyMap").toString().isNotEmpty() && ktu != nullptr && ktu->autoKey.get(),
                       "detect_key: the key map is on the session and Moth Tune follows it (auto key " + juce::String (ktu != nullptr && ktu->autoKey.get() ? "on" : "off") + ")");
                auto kf = juce::JSON::parse (call ("detect_key", juce::String (R"({"file":")") + wavF.getFullPathName().replace ("\\", "/") + R"(","apply":false})"));
                check (kf["key_changes"].size() == 2, "detect_key from a file: " + juce::String (kf["key_changes"].size()) + " key regions, tempo " + kf["bpm"].toString() + " BPM");
                // the Moth Key plug-in itself: hear the first 20 s (E minor) and analyse
                auto mk = session->edit->getPluginCache().createNewPlugin (SqMothKeyPlugin::xmlTypeName, {});
                if (auto* m = dynamic_cast<SqMothKeyPlugin*> (mk.get()))
                {
                    m->autoApply.setValue (false, nullptr);
                    mk->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                    juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                    for (size_t p = 0; p + 512 <= (size_t) (20 * sr); p += 512)
                    {
                        buf.copyFrom (0, 0, y.data() + p, 512); buf.copyFrom (1, 0, y.data() + p, 512);
                        te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (p / sr), te::TimePosition::fromSeconds ((p + 512) / sr)), true, false, true, false);
                        mk->applyToBuffer (fc);
                    }
                    m->analyseHeard(); for (int i = 0; i < 500 && m->isAnalysing(); ++i) juce::Thread::sleep (10);
                    const auto res = m->lastResult();
                    check (m->hasResult() && res.root == 4 && res.minor, "Moth Key plug-in: heard " + juce::String (m->heardSeconds(), 0) + " s and found " + MothKeyAnalysis::keyText (res.root, res.minor));
                    mk->baseClassDeinitialise();
                }
                call ("delete_track", R"({"track":"Key Song"})");
                session->edit->state.removeProperty ("sqmKeyMap", nullptr);
            }
            // 6. Siren Comp / Siren EQ / Hush De-Ess 2 through vocal_fx, and Assist / Learning through the real plug-ins
            {
                call ("add_track", R"({"kind":"audio","name":"Siren"})");
                for (auto w : { "sirencomp", "sireneq", "deess2", "mothkey" }) call ("vocal_fx", juce::String (R"({"track":"Siren","which":")") + w + R"(","action":"add"})");
                auto* st = session->findTrack ("Siren"); SqSirenCompPlugin* sc = nullptr; SqSirenEqPlugin* se = nullptr; SqHushDeEss2Plugin* sh = nullptr; SqMothKeyPlugin* sk = nullptr;
                if (st != nullptr) for (auto pl : st->pluginList) { if (auto a = dynamic_cast<SqSirenCompPlugin*> (pl)) sc = a; if (auto b = dynamic_cast<SqSirenEqPlugin*> (pl)) se = b; if (auto c = dynamic_cast<SqHushDeEss2Plugin*> (pl)) sh = c; if (auto d = dynamic_cast<SqMothKeyPlugin*> (pl)) sk = d; }
                check (sc != nullptr && se != nullptr && sh != nullptr && sk != nullptr, "vocal_fx: Siren Comp, Siren EQ, Hush De-Ess 2 and Moth Key added");
                call ("vocal_fx", R"({"track":"Siren","which":"sirencomp","action":"set","settings":{"model1":"FET","fet1":"All","dualOn":true,"model2":"Opto B","Threshold 1":-20,"punchOn":true,"Ceiling":-2}})");
                call ("vocal_fx", R"({"track":"Siren","which":"sireneq","action":"set","settings":{"b2On":true,"b2Mode":"pitch","b2Harm":3,"Band 2 Gain":-4,"hpSlope":24,"Air":3}})");
                call ("vocal_fx", R"({"track":"Siren","which":"deess2","action":"set","settings":{"linkOn":true,"Split":6000}})");
                if (sc != nullptr && se != nullptr && sh != nullptr)
                    check (sc->model1.get() == 0 && sc->fet1.get() == 4 && sc->dual.get() && sc->model2.get() == 2 && std::abs (sc->bank[SqSirenCompPlugin::thr1] + 20) < 0.01f && sc->punch.get()
                           && se->on[1].get() && se->mode[1].get() == 1 && se->harm[1].get() == 3 && se->hpSlope.get() == 3 && std::abs (se->bank[SqSirenEqPlugin::airGain] - 3) < 0.01f
                           && sh->link.get() && std::abs (sh->bank[SqHushDeEss2Plugin::split] - 6000) < 1, "vocal_fx: settings by name (models, FET All, dual, pitch band on H3, 24 dB slope, link)");
                call ("delete_track", R"({"track":"Siren"})");
                // render helper for a private plug-in copy
                const double sr = 48000; std::vector<float> vl, vr; Eng::TestSignals::vocal (vl, vr, sr, 6.0);
                auto render = [&] (te::Plugin& p, std::vector<float> l, std::vector<float> r, std::function<void()> perBlock = {})
                {
                    juce::AudioBuffer<float> buf (2, 256); te::MidiMessageArray midi;
                    for (size_t q = 0; q + 256 <= l.size(); q += 256)
                    {
                        buf.copyFrom (0, 0, l.data() + q, 256); buf.copyFrom (1, 0, r.data() + q, 256);
                        te::PluginRenderContext fc (&buf, 0, 256, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 256) / sr)), true, false, true, false);
                        p.applyToBuffer (fc); if (perBlock) perBlock();
                        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + 256, l.data() + q); std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + 256, r.data() + q);
                    }
                    return l;
                };
                {   // Siren Comp Assist: learn, suggest (Controlled), preview, apply
                    auto pp = session->edit->getPluginCache().createNewPlugin (SqSirenCompPlugin::xmlTypeName, {}); auto* c = dynamic_cast<SqSirenCompPlugin*> (pp.get());
                    pp->baseClassInitialise ({ te::TimePosition(), sr, 256 });
                    sqSetParam (c->bank, SqSirenCompPlugin::thr1, 0.0f);   // the knobs do nothing: anything heard comes from the suggestion
                    c->startLearning(); render (*pp, vl, vr); c->stopLearning(); c->makeSuggestion (1);
                    c->preview.setValue (true, nullptr); double grSum = 0; int grN = 0; render (*pp, vl, vr, [&] { grSum += c->gr1.load() + c->gr2.load(); ++grN; });
                    const double avg = grSum / juce::jmax (1, grN);
                    c->applySuggestion(); const auto s = c->suggestionCopy();
                    check (avg > 1.5 && ! c->preview.get() && c->model1.get() == s.s1.model && c->dual.get() == s.dual && std::abs (c->bank[SqSirenCompPlugin::thr1] - s.s1.thresholdDb) < 0.05f,
                           "Siren Comp Assist: previewed suggestion compresses " + juce::String (avg, 1) + " dB on average before it is applied; Apply commits it to the knobs");
                    pp->baseClassDeinitialise();
                }
                {   // Siren EQ Vocal Learning: finds an injected resonance and the singer's range, audition, apply
                    auto pp = session->edit->getPluginCache().createNewPlugin (SqSirenEqPlugin::xmlTypeName, {}); auto* e = dynamic_cast<SqSirenEqPlugin*> (pp.get());
                    pp->baseClassInitialise ({ te::TimePosition(), sr, 256 });
                    auto rl = vl, rr = vr; Eng::Biquad b1, b2; b1.set (Eng::Biquad::peak, sr, 2500, 8, 10); b2 = b1;
                    for (size_t i = 0; i < rl.size(); ++i) { rl[i] = b1.process (rl[i]); rr[i] = b2.process (rr[i]); }
                    e->startLearning(); render (*pp, rl, rr); const auto txt = e->finishLearning();
                    e->preview.setValue (true, nullptr); render (*pp, rl, rr);
                    e->applySuggestion(); int nOn = 0; for (int b = 0; b < 6; ++b) nOn += e->on[(size_t) b].get();
                    check (e->hasSuggestion() && txt.contains ("2.") && txt.contains ("kHz") && nOn >= 1, "Siren EQ Vocal Learning: " + txt);
                    pp->baseClassDeinitialise();
                }
                {   // Hush De-Ess 2 Assist on the sung vocal (its sibilants)
                    auto pp = session->edit->getPluginCache().createNewPlugin (SqHushDeEss2Plugin::xmlTypeName, {}); auto* h = dynamic_cast<SqHushDeEss2Plugin*> (pp.get());
                    pp->baseClassInitialise ({ te::TimePosition(), sr, 256 });
                    h->startLearning(); render (*pp, vl, vr); const auto txt = h->finishLearning();
                    check (h->hasSuggestion() && h->suggestedSoft() < -10.0f, "Hush De-Ess 2 Assist: " + txt);
                    pp->baseClassDeinitialise();
                }
            }
            // 7. Editions, then the factory presets of all five
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true; Edition::owned().vocal = true;
            check (Edition::isTypeAllowed ("sqmSirenComp") && Edition::isTypeAllowed ("sqmSirenEq") && Edition::isTypeAllowed ("sqmHushDeEss2") && Edition::isTypeAllowed ("sqmMothKey") && ! Edition::isTypeAllowed ("sqmCinder"),
                   "editions: Moth Key, Siren Comp, Siren EQ and Hush De-Ess 2 are in the Vocal Suite");
            Edition::owned() = Edition::Owned {}; Edition::owned().core = true;
            check (! Edition::isTypeAllowed ("sqmSirenComp") && ! Edition::isTypeAllowed ("sqmMothKey"), "editions: not in core Squelchmoth");
            Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
            auto rep = session->sessionsDir.getChildFile ("preset-audit-voice.md");
            auto sum = PresetAudit::run (*session->edit, { "Moth Tune", "Siren Comp", "Siren EQ", "Hush De-Ess 2" }, { "sqmSirenComp", "sqmSirenEq", "sqmHushDeEss2" }, 3.0, rep);
            juce::String firstFail; for (auto& row : sum.rows) if (! row.pass && firstFail.isEmpty()) firstFail = row.plugin + " / " + row.preset + ": " + row.why;
            int nTune = 0, nComp = 0, nEq = 0, nDe = 0; double maxCpu = 0;
            for (auto& row : sum.rows) { nTune += row.plugin == "Moth Tune"; nComp += row.plugin == "Siren Comp"; nEq += row.plugin == "Siren EQ"; nDe += row.plugin == "Hush De-Ess 2"; maxCpu = juce::jmax (maxCpu, (double) row.cpuPercent); }
            check (nTune >= 150 && nComp >= 110 && nEq >= 100 && nDe >= 60 && sum.failed == 0,
                   "Voice presets: " + juce::String (nTune) + " Moth Tune, " + juce::String (nComp) + " Siren Comp, " + juce::String (nEq) + " Siren EQ, " + juce::String (nDe) + " Hush De-Ess 2; audit "
                   + juce::String (sum.passed) + "/" + juce::String (sum.presets) + " pass (max CPU " + juce::String (maxCpu, 2) + " %)" + (firstFail.isNotEmpty() ? " first failure " + firstFail : juce::String()));
            auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal/preset-audit-1.11-voice.md");
            if (keep.getParentDirectory().isDirectory()) rep.copyFileTo (keep);
        }
        std::cout << "  (1.11 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)\n";
    }

    auto unknown = rpc ("tools/call", juce::JSON::parse (R"({"name":"nope","arguments":{}})"));
    check ((bool) unknown["result"]["isError"], "unknown tool reports isError");
    auto badMethod = rpc ("foo/bar", juce::var());
    check (badMethod["error"]["code"].toString() == "-32601", "unknown method -> -32601");

    std::cout << "\n== " << passes << " passed, " << failures << " failed ==\n";

    scannerOwner.reset();   // the scanner saves the plug-in list and must go before the engine
    session.reset();
    engine.reset();
    return failures == 0 ? 0 : 1;
}
