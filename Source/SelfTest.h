#pragma once
// Headless end-to-end test: drives the DAW exclusively through the MCP JSON-RPC layer,
// exactly the way Claude does, and checks the results. Run:  Squelchmoth --selftest

#include "McpServer.h"
#include "Behaviours.h"
#include "Licence.h"
#include "ChatPanel.h"
#include "VocalRemover.h"
#include "HearingWindow.h"
#include "MixerView.h"
#include "SqEngine111.h"
#include "ScoreWindow.h"

/** Set by Main for "--preset-audit [report.md]": the self-test sets up the engine, runs the audit and returns. */
struct SelfTestMode { static inline bool presetAuditOnly = false, renderFaces = false, siteShots = false; static inline juce::String reportPath, facesDir, facePreset; static inline juce::StringArray only; };

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
    engine->getPluginManager().createBuiltInType<SqMuteAutoPlugin>();
    engine->getPluginManager().createBuiltInType<SqExternalFxPlugin>();
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
    engine->getPluginManager().createBuiltInType<SqSpectraEqPlugin>();
    engine->getPluginManager().createBuiltInType<SqTorqueCompPlugin>();
    engine->getPluginManager().createBuiltInType<SqSummitLimiterPlugin>();
    engine->getPluginManager().createBuiltInType<SqAtriumVerbPlugin>();
    engine->getPluginManager().createBuiltInType<SqApexMB2Plugin>();
    engine->getPluginManager().createBuiltInType<SqSibylDeEssPlugin>();
    engine->getPluginManager().createBuiltInType<SqPortcullisGatePlugin>();
    engine->getPluginManager().createBuiltInType<SqKilnPlugin>();
    engine->getPluginManager().createBuiltInType<SqVoltagePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltagePowerPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageTapePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageConsolePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageClipPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageCompPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageOptoPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageMuPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageBrigadePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageSpringPlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltagePlatePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageBridgePlugin>();
        engine->getPluginManager().createBuiltInType<SqVoltageOverdrivePlugin>();
    engine->getPluginManager().createBuiltInType<SqSpindleDelayPlugin>();
    engine->getPluginManager().createBuiltInType<SqFissureFilterPlugin>();
    engine->getPluginManager().createBuiltInType<SqDuetFilterPlugin>();
    engine->getPluginManager().createBuiltInType<SqSpeckFilterPlugin>();
    engine->getPluginManager().createBuiltInType<SqMotePlugin>();
    engine->getPluginManager().createBuiltInType<SqMirrorwingPlugin>();
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

    if (SelfTestMode::siteShots)
    {
        // Website screenshots: real views, real tools, real content (the chat's replies come from a local stand-in for
        // the OpenAI API so no account is needed; every tool it asks for really runs in the session).
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (SelfTestMode::facesDir.isNotEmpty() ? SelfTestMode::facesDir : juce::String ("siteshots"));
        dir.createDirectory();
        Edition::owned() = Edition::Owned { true, true, true, true, true, true, true };
        auto save = [&] (const juce::Image& img, const juce::String& name)
        {
            auto f = dir.getChildFile (name + ".png"); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
            std::cout << "  " << f.getFullPathName() << " (" << img.getWidth() << " x " << img.getHeight() << ")\n";
        };
        auto snap = [&] (juce::Component& c, int w, int h) { c.setSize (w, h); juce::MessageManager::getInstance()->runDispatchLoopUntil (400); return c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f); };

        // 00. The Video window on a video the app made itself: a real beat under the app's own artwork, with a title and a caption
        const auto shotArt = juce::File::getCurrentWorkingDirectory().getChildFile ("Resources/welcome_banner.jpg");
        if (Frame::ffmpeg().existsAsFile() && shotArt.existsAsFile())
        {
            call ("new_session", R"({"name":"Night Moth"})");
            call ("build_beat", R"({"style":"trap","bpm":140,"seed":7})");
            const auto art = dir.getChildFile ("beat-art.mp4");
            const auto made = call ("video_export", "{\"picture\":\"image\",\"image\":" + juce::JSON::toString (juce::var (shotArt.getFullPathName())) + ",\"width\":1280,\"height\":720,\"start_seconds\":0,\"end_seconds\":14,\"wait_seconds\":400,\"path\":" + juce::JSON::toString (juce::var (art.getFullPathName())) + "}");
            std::cout << "  video shot: " << made.upToFirstOccurrenceOf ("\n", false, false).substring (0, 160) << "\n";
            call ("video_import", "{\"sound\":false,\"path\":" + juce::JSON::toString (juce::var (art.getFullPathName())) + "}");
            call ("video_text", R"({"text":"NIGHT MOTH","style":"title","start_seconds":2.0,"length_seconds":3.5})");
            call ("video_text", R"({"text":"Rich Noise Audio","style":"lower third","start_seconds":6.0,"length_seconds":3.0})");
            call ("video_text", R"({"text":"made, mixed and cut in one window","start_seconds":9.5,"end_seconds":13.0})");
            VideoContent vc; attachVideo (vc, tools, *session); vc.setSize (1100, 720);
            auto settle = [&] (double t)
            {
                // with no audio device running (this mode) only a session's first playhead move holds: the engine's
                // transport timer copies the position back from a playhead that never advances. So one position per session here.
                session->edit->getTransport().setPosition (tracktion::TimePosition::fromSeconds (t));
                for (int i = 0; i < 400; ++i) { juce::MessageManager::getInstance()->runDispatchLoopUntil (100); vc.update(); if (vc.imageShown().isValid() && vc.getStrip().size() > 0 && vc.getStrip().finished() && vc.frameShown() == Frame::frameIndexAt (t, vc.getPlayer().facts().fps)) break; }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (900);     // the full-quality frame replaces the quick one
                std::cout << "  video shot: asked for " << t << " s, the playhead is at " << session->edit->getTransport().getPosition().inSeconds() << " s, frame shown " << vc.frameShown() << "\n";
            };
            settle (10.6); save (snap (vc, 1100, 720), "video");
            call ("video_shape", R"({"shape":"vertical","fit":"fill","focus_x":0.36})");
            settle (3.2); save (snap (vc, 1100, 720), "video-vertical");
            vc.getPlayer().stopStream();
            if (juce::SystemStats::getEnvironmentVariable ("SQUELCH_SHOTS_ONLY", {}) == "video") { scannerOwner.reset(); session.reset(); engine.reset(); return 0; }
        }

        // 0. The Hearing window on a real beat: the note panels, and Unmask measuring two of the beat's own tracks
        {
            call ("new_session", R"({"name":"Hearing"})");
            const auto built = call ("build_beat", R"({"style":"trap","bpm":140,"seed":7})");
            HearingContent hc; attachHearing (hc, tools, *session);
            int a = 0, b = 0, n = 0; juce::StringArray names;
            for (auto* t : session->getTracks()) { ++n; names.add (t->getName()); const auto nm = t->getName().toLowerCase(); if (a == 0 && (nm.contains ("808") || nm.contains ("bass") || nm.contains ("sub"))) a = n; else if (b == 0 && (nm.contains ("drum") || nm.contains ("kick"))) b = n; }
            std::cout << "  hearing shot: build_beat -> " << built.upToFirstOccurrenceOf ("\n", false, false).substring (0, 160) << "\n  tracks: " << names.joinIntoString (" | ") << "\n";
            if (a == 0 || b == 0) { a = juce::jmax (1, n - 1); b = n; }
            hc.setFrequency (SqHear::midiToHz (53)); hc.setSecond (SqHear::midiToHz (53) + 2.0);
            hc.chooseTracks (a, b); hc.measure (false);
            std::cout << "  hearing shot: " << hc.unmaskView().message.upToFirstOccurrenceOf ("\n", false, false) << "\n";
            save (snap (hc, 980, 836), "hearing");
            if (juce::SystemStats::getEnvironmentVariable ("SQUELCH_SHOTS_ONLY", {}) == "hearing") { scannerOwner.reset(); session.reset(); engine.reset(); return 0; }
        }

        // 1. The mixer with ChatGPT working in Squelchmoth Chat
        call ("new_session", R"({"name":"Late Night Drive"})");
        call ("set_tempo", R"({"bpm":92})");
        call ("delete_track", R"({"track":"Synth 1"})");
        {   int n = 1; for (auto t : { "Kick", "Snare", "Bass", "Keys" }) call ("update_track", "{\"track\":\"Audio " + juce::String (n++) + "\",\"name\":\"" + t + "\"}"); }
        call ("add_track", R"json({"kind":"audio","name":"Vocal (Lead)"})json");
        call ("update_track", R"({"track":"Kick","volume_db":-2.0})");
        call ("update_track", R"({"track":"Snare","volume_db":-4.5,"pan":0.1})");
        call ("update_track", R"({"track":"Bass","volume_db":-3.0})");
        call ("update_track", R"({"track":"Keys","volume_db":-7.0,"pan":-0.35})");
        call ("channel_dynamics", R"({"action":"set","track":"Kick","comp":"Cocoon Compressor"})");
        call ("channel_dynamics", R"({"action":"set","track":"Snare","comp":"Apex Dynamics"})");
        call ("channel_dynamics", R"({"action":"set","track":"Bass","comp":"Opto Leveler","limit":"Clamp Limiter"})");
        call ("channel_dynamics", R"({"action":"set","track":"master","limit":"Summit Limiter"})");
        httplib::Server mock;
        mock.Get ("/v1/models", [] (const httplib::Request&, httplib::Response& res)
        { res.set_content (R"({"object":"list","data":[{"id":"gpt-6-astra","object":"model","created":1790000000,"owned_by":"system"}]})", "application/json"); });
        mock.Post ("/v1/responses", [&] (const httplib::Request& req, httplib::Response& res)
        {
            const bool second = juce::String::fromUTF8 (req.body.data(), (int) req.body.size()).contains ("function_call_output");
            if (! second)
                res.set_content (R"({"id":"r1","status":"completed","output":[
                    {"id":"m1","type":"message","role":"assistant","content":[{"type":"output_text","text":"On it - Torque Comp and Summit Limiter on the lead vocal, the meter on RMS, and the fader pulled back a touch."}]},
                    {"id":"f1","type":"function_call","call_id":"c1","name":"channel_dynamics","arguments":"{\"action\":\"set\",\"track\":\"Vocal (Lead)\",\"comp\":\"Torque Comp\",\"limit\":\"Summit Limiter\"}"},
                    {"id":"f2","type":"function_call","call_id":"c2","name":"mixer_layout","arguments":"{\"action\":\"set_meter\",\"track\":\"Vocal (Lead)\",\"meter\":\"rms\"}"},
                    {"id":"f3","type":"function_call","call_id":"c3","name":"update_track","arguments":"{\"track\":\"Vocal (Lead)\",\"volume_db\":-1.5}"}]})", "application/json");
            else
                res.set_content (R"({"id":"r2","status":"completed","output":[{"id":"m2","type":"message","role":"assistant","content":[{"type":"output_text","text":"Done on **Vocal (Lead)**:\n- COMP: Torque Comp (watch the GR meter)\n- LIMIT: Summit Limiter, ceiling -1 dBTP\n- Meter: RMS, fader at -1.5 dB\nWant the same chain on the backing vocals?"}]}]})", "application/json");
        });
        const int port = mock.bind_to_any_port ("127.0.0.1");
        std::thread mockThread ([&] { mock.listen_after_bind(); });
        const auto savedProvider = Prefs::getString ("chat_provider");
        Prefs::set ("chat_apiBase", "http://127.0.0.1:" + juce::String (port));
        Prefs::set ("chat_provider", "chatgpt");
        Prefs::set ("chat_model_openai", "gpt-6-astra");
        Chat::KeyStore::set ("sk-site-shot-key-0000000000", Chat::Provider::chatgpt);
        {
            TimelineView tl (*session); MixerView mx (*session, &tl.state);
            Chat::Panel cp (*session, tools);
            cp.setSize (420, 900); juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            cp.sendText ("Compress the lead vocal, add a limiter and show me its RMS");
            for (int i = 0; i < 200 && cp.getEngine().isBusy(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
            auto left = snap (mx, 1080, 900), right = snap (cp, 420, 900);
            juce::Image both (juce::Image::RGB, 1500, 900, true);
            { juce::Graphics g (both); g.drawImageAt (left, 0, 0); g.drawImageAt (right, 1080, 0); }
            save (both, "chat");
            if (cp.getEngine().chatFile.existsAsFile()) cp.getEngine().chatFile.deleteFile();
            cp.getEngine().clear();
        }
        mock.stop(); mockThread.join();
        Chat::KeyStore::forget (Chat::Provider::chatgpt);
        if (auto f = Prefs::file()) { f->removeValue ("chat_apiBase"); f->removeValue ("chat_model_openai"); }
        Prefs::set ("chat_provider", savedProvider.isNotEmpty() ? savedProvider : juce::String ("claude"));

        // 2. The Channel Rack with a trap beat and a lead line
        call ("new_session", R"({"name":"Chrome Trap"})");
        call ("set_tempo", R"({"bpm":140})");
        call ("rack", R"({"action":"add","type":"kit","what":"Trap"})");
        call ("rack", R"({"action":"write","channel":"Kick","steps":"x.....x...x....."})");
        call ("rack", R"({"action":"write","channel":"Snare","steps":"....x.......x..."})");
        call ("rack", R"({"action":"write","channel":"Clap","steps":"....x.......x..x"})");
        call ("rack", R"({"action":"write","channel":"Closed Hat","steps":"x.x.x.x.x.x3x.x4"})");
        call ("rack", R"({"action":"write","channel":"Open Hat","steps":"......x.......x."})");
        call ("rack", R"({"action":"write","channel":"Perc","steps":"...x......x....."})");
        call ("rack", R"({"action":"write","channel":"Sub 808","steps":"x.....x...x....."})");
        call ("rack", R"({"action":"lane","channel":"Kick","lane":"velocity","values":[118,100,100,100,100,100,104,100,100,100,96,100,100,100,100,100]})");
        call ("rack", R"({"action":"add","type":"instrument","what":"Venom","name":"Lead"})");
        call ("rack", R"({"action":"notes","channel":"Lead","replace":true,"notes":[
            {"pitch":"F4","start_beat":0,"length_beats":0.75,"velocity":100},{"pitch":"G#4","start_beat":0.75,"length_beats":0.75,"velocity":96},
            {"pitch":"C5","start_beat":1.5,"length_beats":1,"velocity":104},{"pitch":"A#4","start_beat":2.5,"length_beats":0.5,"velocity":92},
            {"pitch":"G#4","start_beat":3,"length_beats":1,"velocity":100}]})");
        {
            ChannelRackComponent rk (*session);
            rk.setGraphVisible (true);
            save (snap (rk, 1240, 770), "rack");
        }

        // 3. The Mastering window: a four-song EP made in the app (rack beats + synth chords), bounced, sequenced,
        //    loudness-detected, with a Summit Limiter on the master; the meters fed by the record playing through the
        //    master analyzer tap exactly as playback would.
        {
            struct Song { const char* title; int bpm; const char* kit; const char* genre; const char* synth; const char* chord; };
            const Song songs[] = { { "Neon Nights", 92, "Boom Bap", "boom bap", "Mothsynth 2", "C4+D#4+G4+A#4" },
                                   { "Afterglow", 140, "Trap", "trap", "Venom", "F4+G#4+C5" },
                                   { "Moth Light", 124, "House", "house", "Mirrorwing", "A3+C4+E4+G4" },
                                   { "Static Bloom", 100, "Lo-Fi", "lofi", "Hexwing", "D4+F4+A4+C5" } };
            juce::StringArray files;
            int n = 0;
            for (auto& sg : songs)
            {
                call ("new_session", juce::String ("{\"name\":\"") + sg.title + "\"}");
                call ("set_tempo", "{\"bpm\":" + juce::String (sg.bpm) + "}");
                call ("rack", juce::String ("{\"action\":\"add\",\"type\":\"kit\",\"what\":\"") + sg.kit + "\"}");
                call ("rack", juce::String ("{\"action\":\"starter\",\"genre\":\"") + sg.genre + "\"}");
                call ("rack", juce::String ("{\"action\":\"add\",\"type\":\"instrument\",\"what\":\"") + sg.synth + "\",\"name\":\"Chords\"}");
                juce::String notes;
                for (auto& pitch : juce::StringArray::fromTokens (sg.chord, "+", ""))
                    notes << (notes.isEmpty() ? "" : ",") << "{\"pitch\":\"" << pitch << "\",\"start_beat\":0,\"length_beats\":3.5,\"velocity\":90}";
                call ("rack", "{\"action\":\"notes\",\"channel\":\"Chords\",\"replace\":true,\"notes\":[" + notes + "]}");
                call ("pattern", R"({"action":"place","pattern":1,"bar":1,"repeats":8})");
                auto f = dir.getChildFile ("ep_" + juce::String (++n) + ".wav"); f.deleteFile();
                call ("bounce", "{\"path\":\"" + f.getFullPathName() + "\",\"start_bar\":1,\"end_bar\":9,\"tail_seconds\":1,\"normalize_lufs\":-16}");
                if (f.existsAsFile()) files.add (f.getFullPathName());
            }
            call ("new_session", R"({"name":"Neon Nights EP session"})");
            MasteringProject mp (*session);
            tools.ui.mastering = [&] { return &mp; };
            call ("mastering_new", R"({"name":"Neon Nights EP"})");
            call ("mastering_set_album", R"({"album":"Neon Nights EP","artist":"Rich Noise Audio","genre":"Hip-Hop","year":"2026"})");
            juce::String paths; for (auto& f : files) paths << (paths.isEmpty() ? "" : ",") << "\"" << f << "\"";
            call ("mastering_add_tracks", "{\"paths\":[" + paths + "]}");
            for (int i = 0; i < 4; ++i)
                call ("mastering_set_track", "{\"track\":" + juce::String (i + 1) + ",\"title\":\"" + songs[i].title + "\"" + (i > 0 ? ",\"pause_seconds\":0" : "") + "}");
            call ("mastering_insert", R"({"target":"master","plugin":"Spectra EQ","preset":"Master | Gentle Polish"})");
            call ("mastering_insert", R"({"target":"master","plugin":"Summit Limiter","preset":"Master | Streaming Ready"})");
            call ("mastering_insert", R"({"target":1,"plugin":"Torque Comp"})");
            for (int i = 1; i <= 4; ++i) call ("mastering_detect_loudness", "{\"track\":" + juce::String (i) + "}");
            MasteringContent mc (mp);
            mc.setSize (1280, 820);
            for (int w = 0; w < 30; ++w)   // painting starts the waveform thumbnails; they build in the background
            { mc.createComponentSnapshot (mc.getLocalBounds(), true, 0.25f); juce::MessageManager::getInstance()->runDispatchLoopUntil (200); }
            if (mp.tap != nullptr && files.size() > 1)
            {   // play song 2 through the analyzer tap for 6 seconds, as the transport would
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                if (auto rd = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (juce::File (files[1]))))
                {
                    mp.tap->baseClassInitialise ({ te::TimePosition(), rd->sampleRate, 512 });
                    juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                    const juce::int64 start = (juce::int64) (rd->sampleRate * 4.0), len = (juce::int64) (rd->sampleRate * 6.0);
                    for (juce::int64 pos = start; pos + 512 <= start + len && pos + 512 <= rd->lengthInSamples; pos += 512)
                    {
                        rd->read (&buf, 0, 512, pos, true, true);
                        buf.applyGain (juce::Decibels::decibelsToGain (2.0f));   // about where the limiter leaves it
                        te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds ((double) pos / rd->sampleRate), te::TimePosition::fromSeconds ((double) (pos + 512) / rd->sampleRate)), true, false, true, false);
                        mp.tap->applyToBuffer (fc);
                        if ((pos / 512) % 24 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
                    }
                }
            }
            save (snap (mc, 1280, 820), "mastering");
            tools.ui.mastering = nullptr;
        }
        scannerOwner.reset(); session.reset(); engine.reset();
        return 0;
    }

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
    {
        // 1.12: CPU limits are set on the reference machine; a slower computer gets proportionally more room
        const double ms = PresetAudit::referenceWorkloadMs();
        PresetAudit::cpuBudgetScale() = (float) juce::jlimit (1.0, 3.0, ms / PresetAudit::referenceMachineMs);
        std::cout << "  machine speed: reference workload " << juce::String (ms, 1) << " ms (reference machine " << juce::String (PresetAudit::referenceMachineMs, 0)
                  << " ms) - CPU limits x" << juce::String (PresetAudit::cpuBudgetScale(), 2) << std::endl;
    }
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
        call ("mastering_set_album", R"({"album":"Selftest Album","artist":"Rich Noise Audio","genre":"Test","year":"2026","upc":"0123456789012"})");
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
        // 1.13.1 short keys: a test table built here, so no real key is ever in the program or the log
        {
            auto entry = [] (const juce::String& k, uint8_t product)
            {
                juce::MemoryBlock e (juce::SHA256 (("squelchmoth3:" + k).toUTF8()).getRawData().getData(), 12);
                e.append (&product, 1); return e;
            };
            const juce::String testA = "7K3QM2XRB9TNH4VWZ8PC", testB = "A1B2C3D4E5F6G7H8J9K0";
            juce::Array<juce::MemoryBlock> rows { entry (testA, 3), entry (testB, 2) };
            std::sort (rows.begin(), rows.end(), [] (auto& x, auto& y) { return std::memcmp (x.getData(), y.getData(), 12) < 0; });
            juce::MemoryBlock table; for (auto& r : rows) table.append (r.getData(), r.getSize());
            juce::String p, s;
            auto look = [&] (const juce::String& typed) { p = {}; return Licence::verifyShortKey (Licence::normaliseShortKey (typed), p, s, table.getData(), table.getSize()); };
            check (look ("7K3QM-2XRB9-TNH4V-WZ8PC") && p == "VOCAL" && s.length() == 8, "a short key (23 characters with dashes) is found and names its pack");
            check (look ("7k3qm 2xrb9 tnh4v wz8pc") && p == "VOCAL", "short keys ignore case and spaces");
            check (look ("AIB2C3D4E5F6G7H8J9KO") && p == "COMPLETE", "short keys read O as 0 and I as 1");
            check (! look ("7K3QM-2XRB9-TNH4V-WZ8PD"), "a short key with one character wrong is refused");
            check (! look ("7K3QM-2XRB9-TNH4V") && Licence::normaliseShortKey ("SQM2-AREiM0SQ").isEmpty(), "a short key with characters missing is refused");
            check (BinaryData::keys3_binSize >= 13 * 8001 && BinaryData::keys3_binSize % 13 == 0 && ! Licence::verifyShortKey (Licence::normaliseShortKey (testA), p, s),
                   "the shipped short-key table is intact (" + juce::String (BinaryData::keys3_binSize / 13) + " keys) and holds no test key");
            bool sorted = true;
            for (int i = 1; i < BinaryData::keys3_binSize / 13; ++i)
                if (std::memcmp (BinaryData::keys3_bin + (i - 1) * 13, BinaryData::keys3_bin + i * 13, 12) >= 0) { sorted = false; break; }
            check (sorted, "the shipped short-key table is sorted, so every key is found");
        }
        {
            // a fresh install has no full access: the Demo edition, no time-limited trial
            const auto saved = Edition::owned();
            props.removeValue ("licenceKeys"); props.removeValue ("licenceKey");
            Licence lic (props);
            check (lic.isDemo() && Edition::isDemo() && Edition::editionName() == "Squelchmoth Demo", "a fresh install runs the Demo - nobody gets the full program without a key");
            auto ownerKey = juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_KEY", {});
            if (ownerKey.isNotEmpty())
                check (lic.activate (ownerKey, false).isEmpty() && lic.isLicensed() && Edition::editionName() == "Squelchmoth Complete", "owner key activates against the shipped public key and unlocks everything");
            auto shortKey = juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_SHORT_KEY", {});
            if (shortKey.isNotEmpty())
            {
                lic.clear (false);
                check (lic.activate (shortKey.toLowerCase(), false).isEmpty() && lic.isLicensed() && Edition::owned().color && Edition::owned().vocal,
                       "the short owner key (typed in lower case) unlocks the full program and every pack");
            }
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
        check (! TrackOps::hasMasterFader (*session) && std::abs (session->edit->getMasterVolumePlugin()->getVolumeDb()) < 0.001f, "Delete Master Fader resets it to unity");
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
                double tp = (double) juce::JSON::parse (call ("apex", R"({"track":"ST Lim","which":"limiter","action":"get"})"))["loudness"]["true_peak_max_dbtp"];
                juce::String how = "true peak";
                if (tp < -99.0)
                {   // a slow CI machine can read the meter before its first block arrived: render once more, then fall back to the sample peak
                    call ("analyze_mix", R"({"start_bar":1,"end_bar":3,"tracks":["ST Lim"]})");
                    tp = (double) juce::JSON::parse (call ("apex", R"({"track":"ST Lim","which":"limiter","action":"get"})"))["loudness"]["true_peak_max_dbtp"];
                    if (tp < -99.0) { tp = pk; how = "sample peak (meter idle)"; }
                }
                check (tp <= -0.95 && tp > -3.0, "Apex Limiter holds the -1 dBTP ceiling with +12 dB of input (" + how + " " + juce::String (tp, 2) + " dBTP)");
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
        double cgr = dyn["comp"]["gain_reduction_db"].toString().getDoubleValue();
        for (int attempt = 0; attempt < 3 && cgr >= -3.0; ++attempt)
        {   // slow CI runners (Intel Mac) can read the meter before the render reached it: render again and re-read
            juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
            fGr.deleteFile();
            call ("bounce", "{\"path\":\"" + pathOf (fGr) + "\",\"tracks\":[\"Gtr\"],\"start_bar\":1,\"end_bar\":3,\"tail_seconds\":0}");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            dyn = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Gtr"})"));
            cgr = dyn["comp"]["gain_reduction_db"].toString().getDoubleValue();
        }
        check (cgr < -3.0, "gain reduction: Cocoon reports real GR on a hot signal (" + juce::String (cgr, 1) + " dB, threshold -30 dB, 4:1)");
        call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"Torque Comp","limit":"Summit Limiter"})");
        {
            auto dynS = juce::JSON::parse (call ("channel_dynamics", R"({"action":"get","track":"Gtr"})"));
            check (dynS["comp"]["plugin"].toString() == "Torque Comp" && dynS["limit"]["plugin"].toString() == "Summit Limiter" && (dynS["comp"]["gain_reduction_db"].isDouble() || dynS["comp"]["gain_reduction_db"].isInt()) && (dynS["limit"]["gain_reduction_db"].isDouble() || dynS["limit"]["gain_reduction_db"].isInt()),
                   "console slots: Torque Comp (COMP) and Summit Limiter (LIMIT) load and report gain reduction");
            call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"Cocoon Compressor","limit":"none"})");
        }
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

        //======================================================================
        // 1.14: Squelchmoth Chat with ChatGPT, against a local mock of the OpenAI Responses API
        {
            httplib::Server mock;
            std::atomic<int> posts { 0 };
            std::atomic<bool> sawAnthropicHeader { false }, sawAuth { true }, toolsShapeOk { true }, storeOff { true }, replayOk { false }, outputMatched { false }, errorMarked { false };
            std::atomic<int> toolCount { 0 };
            const juce::String testKey = "sk-proj-selftest-0123456789abcdefQRS";
            mock.Get ("/v1/models", [&] (const httplib::Request& req, httplib::Response& res)
            {
                if (req.get_header_value ("Authorization") != ("Bearer " + testKey).toStdString()) { res.status = 401; res.set_content (R"({"error":{"message":"Incorrect API key provided","type":"invalid_request_error","code":"invalid_api_key"}})", "application/json"); return; }
                res.set_content (R"({"object":"list","data":[{"id":"gpt-6-astra","object":"model","created":1790000000,"owned_by":"system"},{"id":"gpt-6-astra-audio","object":"model","created":1790000001,"owned_by":"system"},{"id":"text-embedding-3-large","object":"model","created":1700000000,"owned_by":"system"},{"id":"gpt-5.6-terra","object":"model","created":1780000000,"owned_by":"system"}]})", "application/json");
            });
            mock.Post ("/v1/responses", [&] (const httplib::Request& req, httplib::Response& res)
            {
                ++posts;
                if (req.has_header ("anthropic-version")) sawAnthropicHeader = true;
                if (req.get_header_value ("Authorization") != ("Bearer " + testKey).toStdString()) sawAuth = false;
                auto body = juce::JSON::parse (juce::String::fromUTF8 (req.body.data(), (int) req.body.size()));
                toolCount = body["tools"].size();
                if (auto tl = body["tools"].getArray())
                    for (auto& t : *tl) if (t["type"].toString() != "function" || t["name"].toString().isEmpty() || ! t["parameters"].isObject() || t.hasProperty ("input_schema")) toolsShapeOk = false;
                if (! (body.hasProperty ("store") && ! (bool) body["store"]) || body["instructions"].toString().isEmpty() || ! body["instructions"].toString().contains ("ChatGPT")) storeOff = false;
                auto input = body["input"];
                bool hasOutput = false;
                if (auto arr = input.getArray())
                    for (auto& it : *arr)
                    {
                        if (it["type"].toString() == "reasoning" && it["encrypted_content"].toString() == "enc-1") replayOk = true;
                        if (it["type"].toString() == "function_call_output")
                        {
                            hasOutput = true;
                            if (it["call_id"].toString() == "call_a" || it["call_id"].toString() == "call_b") outputMatched = true;
                            if (it["call_id"].toString() == "call_b" && it["output"].toString().startsWith ("ERROR")) errorMarked = true;
                        }
                    }
                if (! hasOutput)
                    res.set_content (R"({"id":"resp_1","object":"response","status":"completed","output":[
                        {"id":"rs_1","type":"reasoning","summary":[],"encrypted_content":"enc-1"},
                        {"id":"msg_1","type":"message","status":"completed","role":"assistant","content":[{"type":"output_text","text":"Putting a compressor on the guitar.","annotations":[]}]},
                        {"id":"fc_1","type":"function_call","call_id":"call_a","name":"channel_dynamics","arguments":"{\"action\":\"set\",\"track\":\"Gtr\",\"comp\":\"Opto Leveler\"}","status":"completed"},
                        {"id":"fc_2","type":"function_call","call_id":"call_b","name":"channel_dynamics","arguments":"{\"action\":\"set\",\"track\":\"Nobody\",\"comp\":\"Opto Leveler\"}","status":"completed"}]})", "application/json");
                else
                    res.set_content (R"({"id":"resp_2","object":"response","status":"completed","output":[{"id":"msg_2","type":"message","status":"completed","role":"assistant","content":[{"type":"output_text","text":"Done: Opto Leveler is on Gtr.","annotations":[]}]}]})", "application/json");
            });
            const int port = mock.bind_to_any_port ("127.0.0.1");
            std::thread mockThread ([&] { mock.listen_after_bind(); });
            Prefs::set ("chat_apiBase", "http://127.0.0.1:" + juce::String (port));
            Chat::KeyStore::set (testKey, Chat::Provider::chatgpt);
            check (Chat::KeyStore::get (Chat::Provider::chatgpt) == testKey && ! Chat::KeyStore::has (Chat::Provider::claude)
                   && ! props.getValue ("cl_openaiKey").contains ("sk-proj"), "chatgpt: its key is stored apart from Claude's, not in plain text");
            check (call ("set_preference", R"({"key":"cl_openaiKey","value":"x"})", false).contains ("private"), "chatgpt: set_preference refuses the OpenAI key");

            auto models = Chat::request ("/v1/models", testKey, nullptr, 5000, Chat::Provider::chatgpt);
            juce::StringArray ids, names; Chat::Panel::chatModels (models.json, ids, names);
            check (models.ok() && ids.size() == 2 && ids[0] == "gpt-6-astra" && ids[1] == "gpt-5.6-terra", "chatgpt: the model menu keeps chat models only, newest first (" + ids.joinIntoString (", ") + ")");
            auto bad = Chat::request ("/v1/models", "sk-wrong", nullptr, 5000, Chat::Provider::chatgpt);
            check (! bad.ok() && bad.status == 401 && bad.error.contains ("platform.openai.com"), "chatgpt: a wrong key gives a clear 401 message");

            Chat::Engine engineGpt (tools, *session);
            engineGpt.provider = Chat::Provider::chatgpt; engineGpt.model = "gpt-6-astra";
            juce::StringArray texts; int calls = 0, okResults = 0, errResults = 0; bool finished = false;
            engineGpt.onEvent = [&] (const Chat::Event& e)
            {
                if (e.kind == Chat::Event::assistantText) texts.add (e.text);
                else if (e.kind == Chat::Event::toolCall) ++calls;
                else if (e.kind == Chat::Event::toolResult) (e.ok ? okResults : errResults)++;
                else if (e.kind == Chat::Event::error) texts.add ("ERROR " + e.text);
                else if (e.kind == Chat::Event::finished) finished = true;
            };
            call ("channel_dynamics", R"({"action":"set","track":"Gtr","comp":"none"})");
            check (engineGpt.send ("Put a compressor on the guitar"), "chatgpt: a message starts a turn");
            for (int i = 0; i < 300 && ! finished; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            auto* g3 = session->findTrack ("Gtr");
            auto comp3 = g3 != nullptr ? Console::Model::findDynamics (g3->pluginList, g3, false) : nullptr;
            check (finished && comp3 != nullptr && comp3->getPluginType() == "sqmOpto", "chatgpt: ChatGPT's function_call ran in the session (Opto Leveler is on Gtr) - same full control as Claude");
            check (calls == 2 && okResults == 1 && errResults == 1, "chatgpt: both function calls reported, the failing one as an error");
            check (posts == 2 && texts.size() == 2 && texts[1].contains ("Opto"), "chatgpt: function_call_output went back and the final answer arrived (" + texts.joinIntoString (" | ") + ")");
            check (! sawAnthropicHeader && sawAuth, "chatgpt: Bearer key, no anthropic-version header");
            check (toolsShapeOk && toolCount == (int) tools.getTools().size(), "chatgpt: every Squelchmoth tool is offered as a Responses function tool (" + juce::String (toolCount.load()) + ")");
            check (storeOff && replayOk && outputMatched && errorMarked, "chatgpt: stateless (store false), reasoning replayed, outputs matched by call_id, failures marked");
            // switching provider mid-chat: the same history goes to Claude cleanly
            auto forC = Chat::Engine::forClaude (engineGpt.getMessages());
            bool clean = forC.size() == 4;
            for (auto& m : forC) if (m.hasProperty ("oa_output")) clean = false;
            check (clean && forC[1]["content"][1]["type"].toString() == "tool_use" && forC[1]["content"][1]["id"].toString() == "call_a" && forC[2]["content"][0]["type"].toString() == "tool_result",
                   "chatgpt: switching to Claude mid-chat carries the conversation over in Messages-API shape");
            auto forO = Chat::Engine::forOpenAI (forC);
            check (forO.size() == 7 && forO[2]["type"].toString() == "function_call" && forO[4]["type"].toString() == "function_call_output", "chatgpt: and a Claude conversation converts to Responses input items");
            engineGpt.save();
            check (engineGpt.chatFile.existsAsFile() && ! engineGpt.chatFile.loadFileAsString().contains (testKey), "chatgpt: the conversation is saved without the key");
            engineGpt.chatFile.deleteFile();

            mock.stop();
            mockThread.join();
            Chat::KeyStore::forget (Chat::Provider::chatgpt);
            if (auto f = Prefs::file()) f->removeValue ("chat_apiBase");
            check (! Chat::KeyStore::has (Chat::Provider::chatgpt), "chatgpt: Forget removes the OpenAI key");
            {   // ChatGPT connecting over the MCP link (Developer mode) is named in the activity log
                juce::String sid2; bool n2 = false;
                server.handleBody (R"({"jsonrpc":"2.0","id":901,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"openai-mcp","version":"1.0"}}})", sid2, n2);
                server.handleBody (R"({"jsonrpc":"2.0","id":902,"method":"tools/call","params":{"name":"get_session","arguments":{}}})", sid2, n2);
                auto entries = session->log.getEntries();
                const bool named = ! entries.empty() && entries.back().source == "ChatGPT" && entries.back().text.startsWith ("get_session");
                juce::String sid3; bool n3 = false;
                server.handleBody (R"({"jsonrpc":"2.0","id":903,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"claude-ai","version":"1.0"}}})", sid3, n3);
                server.handleBody (R"({"jsonrpc":"2.0","id":904,"method":"tools/call","params":{"name":"get_session","arguments":{}}})", sid3, n3);
                entries = session->log.getEntries();
                check (named && entries.back().source == "Claude", "chatgpt: MCP calls from ChatGPT are logged as ChatGPT, from claude.ai as Claude");
            }
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
                check (pct < 12 * PresetAudit::cpuBudgetScale(), "Hive Choir: 32 singers (4 voices x 8), Studio engine, " + juce::String (pct, 2) + " % of one core");
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

    // ===================================================================== 1.12 "Finish Line"
    {
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest 112","tempo":120})");
        const double sr = session->edit->engine.getDeviceManager().getSampleRate() > 0 ? session->edit->engine.getDeviceManager().getSampleRate() : 48000.0;
        auto tone = session->sessionsDir.getChildFile ("st112 tone.wav");
        {
            const int len = (int) (sr * 10.0);
            juce::AudioBuffer<float> buf (2, len);
            for (int i = 0; i < len; ++i)
            {
                const float v = (float) (0.4 * std::sin (juce::MathConstants<double>::twoPi * 441.0 * i / sr));
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            tone.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = tone.createOutputStream())
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0)))
                    w->writeFromAudioSampleBuffer (buf, 0, len);
        }
        const auto tonePath = tone.getFullPathName().replace ("\\", "/");
        juce::AudioFormatManager fm112; fm112.registerBasicFormats();
        auto readFile = [&] (const juce::File& f, double& rate)
        {
            juce::AudioBuffer<float> buf;
            if (auto r = std::unique_ptr<juce::AudioFormatReader> (fm112.createReaderFor (f)))
            {
                rate = r->sampleRate; buf.setSize ((int) r->numChannels, (int) r->lengthInSamples);
                r->read (&buf, 0, (int) r->lengthInSamples, 0, true, true);
            }
            return buf;
        };
        auto rmsDb = [] (const juce::AudioBuffer<float>& b, double rate, double t0, double t1)
        {
            const int a = juce::jlimit (0, b.getNumSamples(), (int) (t0 * rate)), z = juce::jlimit (0, b.getNumSamples(), (int) (t1 * rate));
            double e = 0; int n = 0;
            for (int i = a; i < z; ++i) { e += (double) b.getSample (0, i) * b.getSample (0, i); ++n; }
            return juce::Decibels::gainToDecibels ((float) std::sqrt (e / juce::jmax (1, n)), -200.0f);
        };

        // --- A. MP3 export with Squelchmoth's own encoder
        {
            call ("add_track", R"({"kind":"audio","name":"MP3 Src"})");
            call ("import_audio", R"({"track":"MP3 Src","path":")" + tonePath + R"(","start_bar":1})");
            auto r320 = call ("bounce", R"({"name":"st112 mp3","start_seconds":0,"end_seconds":6,"format":"mp3","mp3_bitrate":320,"title":"Selftest Song","artist":"Rich Noise Audio","tracks":["MP3 Src"]})");
            auto rv0 = call ("bounce", R"({"name":"st112 mp3 v0","start_seconds":0,"end_seconds":6,"format":"mp3","mp3_vbr_quality":0,"tracks":["MP3 Src"]})");
            auto f320 = session->bouncesDir.getChildFile ("st112 mp3.mp3"), fv0 = session->bouncesDir.getChildFile ("st112 mp3 v0.mp3");
            double rate = 0;
            auto dec = readFile (f320, rate);
            // align by cross-correlation, then level and correlation against the source tone
            int lag = 0; double bestC = -1e30;
            for (int L = 0; L < 4000 && dec.getNumSamples() > 0; ++L)
            {
                double c = 0; for (int i = 20000; i < 40000; i += 2) c += (double) std::sin (juce::MathConstants<double>::twoPi * 441.0 * i / rate) * dec.getSample (0, i + L);
                if (c > bestC) { bestC = c; lag = L; }
            }
            double xy = 0, xx = 0, yy = 0;
            for (int i = 20000; i < (int) (5.0 * rate) && i + lag < dec.getNumSamples(); ++i)
            {
                const double x = 0.4 * std::sin (juce::MathConstants<double>::twoPi * 441.0 * i / rate), y = dec.getSample (0, i + lag);
                xy += x * y; xx += x * x; yy += y * y;
            }
            const double corr = xy / std::sqrt (xx * yy + 1e-30), lvl = 10.0 * std::log10 ((yy + 1e-30) / (xx + 1e-30));
            // first audio frame after the ID3 tag and the Info frame: its header's bitrate index
            juce::MemoryBlock mb; f320.loadFileAsData (mb);
            auto* d = (const uint8_t*) mb.getData(); const size_t n = mb.getSize();
            size_t id3 = n > 10 && d[0] == 'I' && d[1] == 'D' && d[2] == '3' ? 10 + (size_t) ((d[6] << 21) | (d[7] << 14) | (d[8] << 7) | d[9]) : 0;
            int kbps = 0; bool info = false;
            if (id3 + 4 < n && d[id3] == 0xFF)
            {
                static const int brs[] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
                const int bri = d[id3 + 2] >> 4, sri = (d[id3 + 2] >> 2) & 3; const int pad = (d[id3 + 2] >> 1) & 1;
                const int frameLen = 144 * brs[bri] * 1000 / (sri == 0 ? 44100 : sri == 1 ? 48000 : 32000) + pad;
                info = id3 + 40 < n && (memcmp (d + id3 + 36, "Info", 4) == 0 || memcmp (d + id3 + 21, "Info", 4) == 0);
                const size_t a2 = id3 + (size_t) frameLen;
                if (a2 + 3 < n && d[a2] == 0xFF) kbps = brs[d[a2 + 2] >> 4];
            }
            auto findBytes = [&] (const char* what, size_t len) { for (size_t i = 0; i + len <= std::min<size_t> (n, 4096); ++i) if (memcmp (d + i, what, len) == 0) return true; return false; };
            const bool hasTitle = id3 > 0 && findBytes ("TIT2", 4) && findBytes ("TPE1", 4) && findBytes ("S\0e\0l\0f\0t\0e\0s\0t\0", 16);
            check (dec.getNumSamples() > (int) (5.5 * rate) && corr > 0.99 && std::abs (lvl) < 0.5 && kbps == 320 && info && hasTitle,
                   "MP3 export (own encoder): 320 kb/s CBR decodes with correlation " + juce::String (corr, 4) + ", level " + juce::String (lvl, 2) + " dB, header " + juce::String (kbps)
                   + " kb/s, Info frame and ID3 title / artist tags");
            check (fv0.existsAsFile() && fv0.getSize() < f320.getSize() && rv0.contains ("VBR V0"), "MP3 VBR V0 export (" + juce::String (fv0.getSize() / 1024) + " KB vs " + juce::String (f320.getSize() / 1024) + " KB at 320)");
            check (r320.contains ("MP3 320 kb/s") && ! f320.getSiblingFile ("st112 mp3 (mp3 source).wav").existsAsFile(), "bounce reports the MP3 settings and removes its temporary WAV");
            call ("delete_track", R"({"track":"MP3 Src"})");
        }

        // --- B. Mute automation lane
        call ("add_track", R"({"kind":"audio","name":"M1"})");
        call ("import_audio", R"({"track":"M1","path":")" + tonePath + R"(","start_bar":1})");
        if (auto* m1 = session->findTrack ("M1"))
        {
            MixCtl::setView (*session, *m1, "mute");
            auto* ma = Session::getMuteAuto (*m1);
            check (ma != nullptr && MixCtl::viewParam (*session, *m1) == ma->mute.get() && MixCtl::viewLabel (*session, *m1) == "mute"
                   && ! Session::getInserts (m1->pluginList, m1).contains (ma) && m1->pluginList.indexOf (ma) == m1->pluginList.indexOf (m1->getVolumePlugin()) - 1,
                   "Mute lane: Track View shows it, hidden from the inserts, just before the fader");
            call ("write_automation", R"({"track":"M1","target":"mute","points":[{"seconds":4.0,"value":true},{"seconds":6.0,"value":false}]})");
            call ("bounce", R"({"name":"st112 mute","start_seconds":0,"end_seconds":8,"bit_depth":32,"dither":false})");
            double rate = 0;
            auto mb = readFile (session->bouncesDir.getChildFile ("st112 mute.wav"), rate);
            const float before = rmsDb (mb, rate, 1.0, 3.9), during = rmsDb (mb, rate, 4.05, 5.95), after = rmsDb (mb, rate, 6.05, 7.9);
            float maxStep = 0, toneStep = (float) (0.4 * 2.0 * std::sin (juce::MathConstants<double>::pi * 441.0 / juce::jmax (1.0, rate))) * 1.15f;
            for (int i = (int) (3.95 * rate); i < (int) (6.05 * rate) && i < mb.getNumSamples(); ++i)
                if (i > 0) maxStep = juce::jmax (maxStep, std::abs (mb.getSample (0, i) - mb.getSample (0, i - 1)));
            check (mb.getNumSamples() > 0 && std::abs (before - after) < 0.1f && during < -100.0f && maxStep <= toneStep,
                   "Mute lane: muted 4-6 s renders silence (" + juce::String (during, 1) + " dB) and full level either side ("
                   + juce::String (before, 1) + " / " + juce::String (after, 1) + " dB), no click at the edges");
            // the Pencil on a mute lane draws blocks
            auto& curve = ma->mute->getCurve();
            curve.clear (nullptr);
            MuteLane::paint (curve, te::TimePosition::fromSeconds (1.0), te::TimePosition::fromSeconds (2.0), true, nullptr);
            MuteLane::paint (curve, te::TimePosition::fromSeconds (2.0), te::TimePosition::fromSeconds (2.5), true, nullptr);
            check (! MuteLane::valueAt (curve, te::TimePosition::fromSeconds (0.99), 0.0f) && MuteLane::valueAt (curve, te::TimePosition::fromSeconds (1.0), 0.0f)
                   && MuteLane::valueAt (curve, te::TimePosition::fromSeconds (2.25), 0.0f) && ! MuteLane::valueAt (curve, te::TimePosition::fromSeconds (2.52), 0.0f)
                   && curve.getNumPoints() <= 4,
                   "Mute lane: the Pencil paints on / off blocks with sharp edges (" + juce::String (curve.getNumPoints()) + " points)");
            auto cl = call ("clear_automation", R"({"track":"M1","target":"mute"})");
            check (curve.getNumPoints() == 0, "Mute lane: clear_automation target mute");
        }
        // --- C. Sync point
        {
            call ("sync_point", R"({"track":"M1","clip":1,"at_seconds_in_clip":1.25})");
            call ("spot_clip", R"({"track":"M1","clip":1,"to_seconds":10.0,"anchor":"sync"})");
            auto* m1 = dynamic_cast<te::AudioTrack*> (session->findTrack ("M1"));
            te::Clip* c = m1 != nullptr && ! m1->getClips().isEmpty() ? m1->getClips()[0] : nullptr;
            const double st1 = c != nullptr ? c->getPosition().getStart().inSeconds() : -1;
            check (c != nullptr && std::abs (st1 - 8.75) < 1e-6, "Sync point: a clip with its sync point 1.25 s in, spotted to 10.0 s, starts at " + juce::String (st1, 4) + " s");
            if (c != nullptr)
            {
                auto pos = c->getPosition();
                c->setPosition ({ { pos.getStart() + te::TimeDuration::fromSeconds (0.5), pos.getEnd() }, pos.getOffset() + te::TimeDuration::fromSeconds (0.5) });
                auto sp = EditOps::syncPointTime (*c);
                auto g = juce::JSON::parse (call ("sync_point", R"({"track":"M1","clip":1})"));
                check (sp && std::abs (sp->inSeconds() - 10.0) < 1e-6 && std::abs ((double) g["sync_point_in_clip_seconds"] - 0.75) < 1e-6,
                       "Sync point stays on the same sound when the clip start is trimmed (10.0 s, 0.75 s into the clip)");
                call ("sync_point", R"({"track":"M1","clip":1,"action":"clear"})");
                check (! EditOps::hasSyncPoint (*c), "Sync point: clear");
            }
        }
        // --- D. Spectrum: 1/12-octave bands, waterfall / sonogram history
        {
            AnalysisHub hub; hub.prepare (48000.0); hub.update();
            SpectrumHistory hist;
            juce::AudioBuffer<float> sb (2, 4096);
            for (int blk = 0; blk < 8; ++blk)
            {
                for (int i = 0; i < 4096; ++i) { const float v = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (blk * 4096 + i) / 48000.0); sb.setSample (0, i, v); sb.setSample (1, i, v); }
                hub.push (sb, 0, 4096); hub.update(); hist.update (hub.spectrum, hub.sr);
            }
            auto bands = SpectrumBands::compute (hub.spectrum, hub.sr, 12);
            const SpectrumBands::Band* at1k = nullptr; const SpectrumBands::Band* up2 = nullptr; const SpectrumBands::Band* dn2 = nullptr;
            for (auto& b : bands)
            {
                if (std::abs (b.centre - 1000.0f) < 1.0f) at1k = &b;
                if (std::abs (b.centre - 1000.0f * std::pow (2.0f, 2.0f / 12.0f)) < 1.0f) up2 = &b;
                if (std::abs (b.centre - 1000.0f * std::pow (2.0f, -2.0f / 12.0f)) < 1.0f) dn2 = &b;
            }
            const float want = juce::Decibels::gainToDecibels (0.25f);
            check (at1k != nullptr && up2 != nullptr && dn2 != nullptr && std::abs (at1k->db - want) < 0.5f && at1k->db - up2->db > 20.0f && at1k->db - dn2->db > 20.0f,
                   "Spectrum 1/12 octave: a 1 kHz sine at " + juce::String (want, 1) + " dBFS reads " + (at1k ? juce::String (at1k->db, 2) : juce::String ("-")) + " dB in its band, "
                   + (at1k && up2 && dn2 ? juce::String (at1k->db - juce::jmax (up2->db, dn2->db), 1) : juce::String ("-")) + " dB above the bands two away");
            auto b3 = SpectrumBands::compute (hub.spectrum, hub.sr, 3);
            float b3db = -200.0f; for (auto& b : b3) if (std::abs (b.centre - 1000.0f) < 1.0f) b3db = b.db;
            check (std::abs (b3db - want) < 0.5f && (int) b3.size() == 31 && (int) bands.size() == 121, "Spectrum 1/3 octave reads the same sine at " + juce::String (b3db, 2) + " dB (31 ISO bands; 1/12: " + juce::String ((int) bands.size()) + ")");
            int brightest = 0;
            if (! hist.rows.empty()) for (int c = 1; c < SpectrumHistory::columns; ++c) if (hist.rows.back()[(size_t) c] > hist.rows.back()[(size_t) brightest]) brightest = c;
            const float bf = 0.5f * (SpectrumHistory::columnHz (brightest) + SpectrumHistory::columnHz (brightest + 1));
            check ((int) hist.rows.size() == 8 && std::abs (std::log2 (bf / 1000.0f)) < 1.0f / 24.0f,
                   "Waterfall / sonogram: one row per frame (" + juce::String ((int) hist.rows.size()) + "), brightest row at " + juce::String (bf, 0) + " Hz");
        }
        // --- E. External Effect: ping through a software loop (the same bridge code the audio device runs)
        {
            auto& bridge = ExternalFxBridge::get();
            bridge.manualOnly = true;
            auto pp = session->edit->engine.getPluginManager().createNewPlugin (*session->edit, SqExternalFxPlugin::xmlTypeName, {});
            auto* fx = dynamic_cast<SqExternalFxPlugin*> (pp.get());
            if (fx != nullptr)
            {
                const double rate = 48000.0; const int block = 256, loopDelay = 300;
                fx->setPairs (2, 2);
                fx->baseClassInitialise ({ te::TimePosition(), rate, block });
                // the "outboard gear": outputs 3-4 -> a 300-sample cable -> inputs 3-4
                std::vector<float> cable ((size_t) (loopDelay + block * 4), 0.0f); size_t cw = 0;
                juce::AudioBuffer<float> devIn (4, block), devOut (4, block), buf (2, block); te::MidiMessageArray midi;
                double clock = 0;
                auto runBlock = [&] (const std::function<float (int)>& src, std::vector<float>* outL)
                {
                    for (int i = 0; i < block; ++i) { const float v = src ? src (i) : 0.0f; buf.setSample (0, i, v); buf.setSample (1, i, v); }
                    te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (clock / rate), te::TimePosition::fromSeconds ((clock + block) / rate)), true, false, false, false);
                    fx->applyToBuffer (fc);
                    if (outL) for (int i = 0; i < block; ++i) outL->push_back (buf.getSample (0, i));
                    devOut.clear();
                    // what arrives at the inputs this block: what left the outputs loopDelay samples ago
                    for (int i = 0; i < block; ++i)
                    {
                        const size_t rd = (cw + cable.size() + (size_t) i - (size_t) loopDelay) % cable.size();
                        devIn.setSample (2, i, cable[rd]); devIn.setSample (3, i, cable[rd]);
                    }
                    const float* ins[4] = { devIn.getReadPointer (0), devIn.getReadPointer (1), devIn.getReadPointer (2), devIn.getReadPointer (3) };
                    float* outs[4] = { devOut.getWritePointer (0), devOut.getWritePointer (1), devOut.getWritePointer (2), devOut.getWritePointer (3) };
                    bridge.processDeviceBlock (ins, 4, outs, 4, block);
                    for (int i = 0; i < block; ++i) cable[(cw + (size_t) i) % cable.size()] = devOut.getSample (2, i);
                    cw = (cw + (size_t) block) % cable.size();
                    clock += block;
                };
                for (int k = 0; k < 8; ++k) runBlock ({}, nullptr);
                fx->startPing();
                for (int k = 0; k < (int) (1.2 * rate / block) && fx->isPinging(); ++k) runBlock ({}, nullptr);
                const int measured = fx->lastPing();
                check (measured == loopDelay + block, "External Effect: Ping measures the round trip to the sample (" + juce::String (measured) + " = " + juce::String (loopDelay) + "-sample loop + one "
                       + juce::String (block) + "-sample device block)");
                if (measured > 0) fx->applyPing (measured);
                // 100 % wet through the loop vs the dry input delayed by the reported latency
                std::vector<float> outL, inL;
                auto sine = [&] (int i) { const float v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 997.0 * (clock + i) / rate); inL.push_back (v); return v; };
                for (int k = 0; k < 200; ++k) runBlock (sine, &outL);
                double e = 0, r = 0; const int L = fx->latencySamples.get();
                for (size_t i = (size_t) L + 4096; i < outL.size(); ++i) { const double d = outL[i] - inL[i - (size_t) L]; e += d * d; r += (double) inL[i - (size_t) L] * inL[i - (size_t) L]; }
                const double nullDb = 10.0 * std::log10 (juce::jmax (1.0e-20, e) / juce::jmax (1.0e-20, r));
                check (L > 0 && nullDb < -60.0 && std::abs (fx->getLatencySeconds() * rate - L) < 0.5,
                       "External Effect: compensated return nulls against the dry signal (" + juce::String (nullDb, 1) + " dB) and reports " + juce::String (L) + " samples to delay compensation");
                fx->mixPct = 50.0f;
                std::vector<float> o2; inL.clear(); outL.clear();
                for (int k = 0; k < 60; ++k) runBlock (sine, &o2);
                double e2 = 0, r2 = 0;
                for (size_t i = 4096; i < o2.size(); ++i) { const double d = o2[i] - inL[i - (size_t) L]; e2 += d * d; r2 += (double) inL[i - (size_t) L] * inL[i - (size_t) L]; }
                check (10.0 * std::log10 (juce::jmax (1.0e-20, e2) / juce::jmax (1.0e-20, r2)) < -60.0, "External Effect: 50 % mix stays phase-aligned (dry and return add without comb filtering)");
                fx->baseClassDeinitialise();
            }
            else check (false, "External Effect plug-in creates");
            pp = nullptr;
            bridge.manualOnly = false;
            auto ex = juce::JSON::parse (call ("external_fx", R"({"track":"M1","action":"add","out_pair":5,"in_pair":7,"send_db":-3})"));
            check (ex["outputs"].toString() == "5-6" && ex["inputs"].toString() == "7-8" && std::abs ((double) ex["send_db"] + 3.0) < 1e-6,
                   "external_fx: add on a track with outputs 5-6 / inputs 7-8 and send -3 dB");
            call ("external_fx", R"({"track":"M1","action":"remove"})");
        }
        // --- F. Mastering track macros + the spectrum over MCP
        {
            MasteringProject mp (*session);
            tools.ui.mastering = [&] { return &mp; };
            call ("mastering_new", R"({"name":"Selftest 112 Album"})");
            const auto tp = tone.getFullPathName().replace ("\\", "\\\\");
            call ("mastering_add_tracks", "{\"paths\":[\"" + tp + "\",\"" + tp + "\",\"" + tp + "\"]}");
            call ("mastering_insert", R"({"target":1,"plugin":"Prism EQ"})");
            call ("mastering_insert", R"({"target":1,"plugin":"Clamp Limiter"})");
            call ("mastering_set_track", R"({"track":1,"fader_db":-2.5,"fade_in":0.4,"fade_out":1.2})");
            auto sv = call ("mastering_macro", R"({"action":"save","name":"Selftest 112 Chain","track":1})");
            auto ap = call ("mastering_macro", R"({"action":"apply","name":"Selftest 112 Chain","tracks":[2,3]})");
            auto tr = mp.tracks();
            auto sig = [&] (te::AudioTrack* t)
            {
                juce::String s2;
                for (auto pl : Session::getInserts (t->pluginList, t)) { mp.edit->flushPluginStateIfNeeded (*pl); auto c = pl->state.createCopy(); c.removeProperty (te::IDs::id, nullptr); s2 << c.toXmlString().hashCode64() << ";"; }
                if (auto vp = t->getVolumePlugin()) s2 << juce::String (vp->getVolumeDb(), 2);
                if (auto c = MasteringProject::clipOf (*t)) s2 << "|" << juce::String (c->getFadeIn().inSeconds(), 2) << "|" << juce::String (c->getFadeOut().inSeconds(), 2);
                return s2;
            };
            check (tr.size() == 3 && Session::getInserts (tr[1]->pluginList, tr[1]).size() == 2 && sig (tr[0]) == sig (tr[1]) && sig (tr[0]) == sig (tr[2]) && sv.contains ("2 rack plug-in"),
                   "Mastering track macro applied to three tracks gives identical racks, fader (-2.5 dB) and fades (" + (tr.size() == 3 ? sig (tr[2]).fromLastOccurrenceOf (";", false, false) : juce::String()) + ")");
            auto ls = juce::JSON::parse (call ("mastering_macro", R"({"action":"list"})"));
            bool listed = false; if (auto arr = ls["macros"].getArray()) for (auto& v : *arr) listed = listed || v.toString() == "Selftest 112 Chain";
            call ("mastering_macro", R"({"action":"delete","name":"Selftest 112 Chain"})");
            check (listed && ! MasteringMacros::fileFor ("Selftest 112 Chain").existsAsFile(), "mastering_macro list / delete");
            auto spj = juce::JSON::parse (call ("mastering_spectrum", R"({"resolution":"twelfth"})"));
            check (spj["bands"].isArray() && spj["bands"].size() == 121, "mastering_spectrum reports 121 1/12-octave bands");
            tools.ui.mastering = nullptr;
        }
        std::cout << "  (1.12 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)\n";
    }

    // ===================================================================== 1.13 "Score"
    {
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Score","tempo":96})");
        call ("add_track", R"({"kind":"instrument","name":"Melody","instrument":"4osc"})");
        auto w = call ("score", R"({"action":"write","track":"Melody","bar":1,"notes":["C4 quarter","E4 eighth","F#4 eighth","G4 half","[C4 E4 G4] dotted half","rest quarter","D5 triplet eighth","E5 triplet eighth","F5 triplet eighth","G5 quarter","Bb4 half"]})");
        auto rd = call ("score", R"({"action":"read","track":"Melody"})");
        const juce::String want = "Bar 1 (4/4): C4 quarter E4 eighth F#4 eighth G4 half\nBar 2: [C4 E4 G4] dotted half rest quarter\nBar 3: D5 triplet eighth E5 triplet eighth F5 triplet eighth G5 quarter Bb4 half";
        check (rd.contains (want), "score write + read: spelled names, dotted chord, rest, triplets (" + rd.fromFirstOccurrenceOf ("Bar 1", true, false).upToFirstOccurrenceOf ("\n\n", false, false).replace ("\n", " / ") + ")");
        call ("score", R"({"action":"set","key":"F major","staves":"grand","track":"Melody"})");
        auto rd2 = call ("score", R"({"action":"read","track":"Melody"})");
        check (rd2.contains ("Key F major, grand staff") && rd2.contains ("Bb4 half"), "score set: key F major and grand staff");
        call ("score", R"({"action":"set","key":"session","staves":"auto","track":"Melody"})");
        // MusicXML out and back in
        auto xmlFile = session->bouncesDir.getChildFile ("st113.musicxml");
        auto ex = call ("score", juce::String (R"({"action":"export","format":"musicxml","track":"Melody","path":")") + xmlFile.getFullPathName().replace ("\\", "/") + R"("})");
        auto im = call ("score", juce::String (R"({"action":"import","path":")") + xmlFile.getFullPathName().replace ("\\", "/") + R"("})");
        const auto importedName = im.fromFirstOccurrenceOf ("as tracks: ", false, false).trim();
        juce::String rd3;
        for (auto t : session->getTracks()) if (t->getName() == importedName && t != session->findTrack ("Melody")) { auto parts = ScoreModel::build (*session, { t }); if (! parts.empty()) rd3 = Notation::toText (parts[0].score); }
        check (xmlFile.existsAsFile() && juce::XmlDocument::parse (xmlFile) != nullptr && im.contains ("Imported 1 part") && rd3.contains (want),
               "MusicXML export -> import gives the same score on a new track");
        // PDF and PNG
        auto pdf = session->bouncesDir.getChildFile ("st113.pdf"), png = session->bouncesDir.getChildFile ("st113.png");
        auto ep = call ("score", juce::String (R"({"action":"export","format":"pdf","track":"Melody","path":")") + pdf.getFullPathName().replace ("\\", "/") + R"("})");
        auto eg = call ("score", juce::String (R"({"action":"export","format":"png","path":")") + png.getFullPathName().replace ("\\", "/") + R"("})");
        juce::MemoryBlock pm; pdf.loadFileAsData (pm);
        auto img = juce::ImageFileFormat::loadFrom (png);
        check (pm.getSize() > 10000 && memcmp (pm.getData(), "%PDF-1.4", 8) == 0 && ep.contains ("1 page(s)") && img.isValid() && img.getHeight() > img.getWidth(),
               "score export: PDF (" + juce::String ((int) (pm.getSize() / 1024)) + " KB, 1 page) and a portrait PNG page (" + juce::String (img.getWidth()) + "x" + juce::String (img.getHeight()) + ")");
        // the page really has ink on it: dark pixels in the staff area
        int dark = 0;
        if (img.isValid()) for (int y = 0; y < img.getHeight(); y += 3) for (int x = 0; x < img.getWidth(); x += 3) if (img.getPixelAt (x, y).getBrightness() < 0.4f) ++dark;
        check (dark > 800, "score page is engraved (" + juce::String (dark) + " ink samples)");
        if (auto keep = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/internal"); keep.isDirectory()) png.copyFileTo (juce::File ("/tmp/score_selftest.png"));
        // edits through the model (what the window does): transpose, move, delete, add
        {
            auto* mel = session->findTrack ("Melody");
            auto parts = ScoreModel::build (*session, { mel });
            std::vector<ScoreModel::NoteRef> first { parts[0].refs[0] };
            ScoreModel::transpose (*session, first, 2);
            ScoreModel::addNote (*session, *mel, 16.0, 1.0, 72);
            auto after = call ("score", R"({"action":"read","track":"Melody"})");
            check (after.contains ("Bar 1 (4/4): D4 quarter") && after.contains ("Bar 5: C5 quarter"), "score edits: transpose a note, enter a note in an empty bar");
        }
        {
            // the Score window itself, drawn offscreen (what the user sees)
            auto sc = std::make_unique<ScoreContent> (*session);
            sc->setSize (1180, 700);
            sc->getView().zoom = 1.3f; sc->getView().relayout();
            auto snap = sc->createComponentSnapshot (sc->getLocalBounds());
            juce::File ("/tmp/score_window.png").deleteFile();
            { juce::FileOutputStream os (juce::File ("/tmp/score_window.png")); juce::PNGImageFormat().writeImageToStream (snap, os); }
            check (snap.isValid() && sc->getView().summary().contains ("2 part(s)"), "Score window: " + sc->getView().summary());
        }
        std::cout << "  (1.13 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Spectra EQ
    {
        std::cout << "\n-- 1.14 Studio Line: Spectra EQ --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Spectra"})");
        call ("add_track", R"({"kind":"audio","name":"EQ"})");
        auto added = call ("spectra_eq", R"({"track":"EQ","action":"add"})");
        SqSpectraEqPlugin* eq = nullptr;
        if (auto tt = session->findTrack ("EQ")) for (auto pl : tt->pluginList) if (auto e = dynamic_cast<SqSpectraEqPlugin*> (pl)) eq = e;
        check (eq != nullptr && added.contains ("Spectra EQ"), "Spectra EQ inserted by spectra_eq add");
        if (eq != nullptr)
        {
            call ("spectra_eq", R"({"track":"EQ","action":"band","shape":"bell","freq":3000,"gain":9,"q":2})");
            call ("spectra_eq", R"({"track":"EQ","action":"band","shape":"high shelf","freq":12000,"gain":-6,"q":0.707})");
            call ("spectra_eq", R"({"track":"EQ","action":"band","shape":"low cut","freq":40,"slope":24})");
            auto g = call ("spectra_eq", R"({"track":"EQ","action":"get"})");
            int bell = -1, shelf = -1, cut = -1;
            for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i) { const auto b = eq->band (i); if (! b.enabled) continue; if (b.shape == SqSpectra::Shape::bell) bell = i; if (b.shape == SqSpectra::Shape::highShelf) shelf = i; if (b.shape == SqSpectra::Shape::lowCut) cut = i; }
            check (bell >= 0 && shelf >= 0 && cut >= 0 && g.contains ("curve_db") && eq->band (cut).slope == 24 && bell == 0,
                   "spectra_eq: three bands made by name (bell, high shelf, 24 dB/oct low cut) and read back with the curve");
            check (bell >= 0 && shelf >= 0 && cut >= 0 && std::abs (eq->curveDb (3000) - (eq->bandDb (bell, 3000) + eq->bandDb (shelf, 3000) + eq->bandDb (cut, 3000))) < 0.01 && std::abs (eq->curveDb (3000) - 9.0) < 0.3,
                   "Spectra EQ: the curve is +" + juce::String (eq->curveDb (3000), 2) + " dB at 3 kHz");

            // render through a separate instance with the same state (the session's copy keeps playing)
            auto measure = [&] (te::Plugin& p, double hz, float amp) -> double
            {
                p.baseClassInitialise ({ te::TimePosition(), 48000.0, 256 });
                const int N = 48000; juce::AudioBuffer<float> buf (2, 256); te::MidiMessageArray midi;
                double si = 0, co = 0; int counted = 0;
                for (int q = 0; q + 256 <= N; q += 256)
                {
                    for (int i = 0; i < 256; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / 48000.0); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                    te::PluginRenderContext fc (&buf, 0, 256, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 48000.0), te::TimePosition::fromSeconds ((q + 256) / 48000.0)), true, false, true, false);
                    p.applyToBuffer (fc);
                    if (q >= N / 2)
                        for (int i = 0; i < 256; ++i) { const double t = juce::MathConstants<double>::twoPi * hz * (q + i) / 48000.0; si += buf.getSample (0, i) * std::sin (t); co += buf.getSample (0, i) * std::cos (t); ++counted; }
                }
                p.baseClassDeinitialise();
                return 20.0 * std::log10 (2.0 * std::sqrt (si * si + co * co) / counted / amp + 1e-12);
            };
            auto twin = session->edit->getPluginCache().createNewPlugin (SqSpectraEqPlugin::xmlTypeName, {});
            auto* tw = dynamic_cast<SqSpectraEqPlugin*> (twin.get());
            if (tw != nullptr)
            {
                tw->restorePluginStateFromValueTree (eq->state);
                double worst = 0; juce::String at;
                for (double hz : { 100.0, 1000.0, 3000.0, 6000.0, 12000.0, 18000.0 }) { const double e = std::abs (measure (*tw, hz, 0.1f) - tw->curveDb (hz)); if (e > worst) { worst = e; at = juce::String ((int) hz); } }
                check (worst < 0.3, "Spectra EQ (Zero Latency) rendered: within " + juce::String (worst, 3) + " dB of its analog curve, 100 Hz-18 kHz (worst at " + at + " Hz), latency 0");
                for (int mode : { 1, 2 })
                {
                    tw->phaseMode.setValue (mode, nullptr);
                    double w2 = 0; for (double hz : { 100.0, 3000.0, 12000.0 }) w2 = juce::jmax (w2, std::abs (measure (*tw, hz, 0.1f) - tw->curveDb (hz)));
                    const int lat = juce::roundToInt (tw->getLatencySeconds() * 48000.0);
                    check (w2 < (mode == 1 ? 0.15 : 0.1) && lat == (mode == 1 ? 47 : 4096 + 256), juce::String (mode == 1 ? "Natural" : "Linear") + " phase rendered within " + juce::String (w2, 3) + " dB of the curve; latency " + juce::String (lat) + " samples reported to the engine");
                }
                tw->phaseMode.setValue (0, nullptr);
                // a dynamic band: loud 3 kHz pulls it down, quiet doesn't
                const int bb = juce::jmax (0, bell);
                tw->props[(size_t) bb].dyn.setValue (true, nullptr);
                tw->bank.params[(size_t) (bb * SqSpectraEqPlugin::perBand + SqSpectraEqPlugin::pThresh)]->setParameter (-30.0f, juce::dontSendNotification);
                tw->bank.params[(size_t) (bb * SqSpectraEqPlugin::perBand + SqSpectraEqPlugin::pRange)]->setParameter (-6.0f, juce::dontSendNotification);
                const double loud = measure (*tw, 3000, 0.5f), quiet = measure (*tw, 3000, 0.003f);
                check (std::abs ((loud - quiet) + 6.0) < 0.5, "Spectra EQ dynamic band: a loud 3 kHz tone is pulled " + juce::String (loud - quiet, 2) + " dB below a quiet one (range -6)");
            }
            // mid / side and undo
            call ("spectra_eq", R"({"track":"EQ","action":"band","band":4,"shape":"high shelf","freq":8000,"gain":4,"placement":"side"})");
            check (eq->band (3).place == SqSpectra::Place::side && eq->band (3).enabled, "spectra_eq: a side-only high shelf");
            call ("undo", "{}");
            check (! eq->band (3).enabled, "one undo removes the band Claude just added");
            // presets
            int nPre = 0, nAdv = 0; for (auto& e : Signature::entries()) if (juce::String (e.type) == "sqmSpectraEq") { nPre = (int) e.presets.size(); for (auto& pr : e.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("spectra_eq", R"({"track":"EQ","action":"preset","preset":"Vocal | Clarity"})");
            int on = 0; for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i) on += eq->band (i).enabled ? 1 : 0;
            check (nPre >= 30 && nAdv >= 8 && pl.contains ("Loaded") && on == 4, "Spectra EQ: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); 'Vocal | Clarity' loads its 4 bands");
            check (Edition::packOf ("sqmSpectraEq") == Edition::Pack::mastering, "Spectra EQ belongs to the Mastering Suite");
            // the window, offscreen
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = eq->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*eq);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                { juce::FileOutputStream os (juce::File ("/tmp/spectra_window.png")); os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (snap, os); }
                check (snap.isValid() && snap.getWidth() >= 1000, "Spectra EQ window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Torque Comp
    {
        std::cout << "\n-- 1.14 Studio Line: Torque Comp --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Torque"})");
        call ("add_track", R"({"kind":"audio","name":"Comp"})");
        auto added = call ("torque_comp", R"({"track":"Comp","action":"add"})");
        SqTorqueCompPlugin* tq = nullptr;
        if (auto tt = session->findTrack ("Comp")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqTorqueCompPlugin*> (pl)) tq = c;
        check (tq != nullptr && added.contains ("Torque Comp"), "Torque Comp inserted by torque_comp add");
        if (tq != nullptr)
        {
            auto st = call ("torque_comp", R"({"track":"Comp","action":"set","style":"variable mu","threshold":-24,"ratio":4,"knee":0,"attack":5,"release":100,"routing":"mid only","colour":"warm","oversampling":8})");
            check (tq->style.get() == 4 && std::abs (tq->bank[SqTorqueCompPlugin::threshold] + 24.0f) < 0.01f && tq->routing.get() == 1 && tq->characterMode.get() == 2 && tq->oversample.get() == 3 && st.contains ("Variable Mu"),
                   "torque_comp set: style by name, threshold, Mid Only, warm colour, 8x oversampling");
            call ("torque_comp", R"({"track":"Comp","action":"set","style":"clean","routing":"stereo","colour":"off","threshold":-24,"ratio":4,"knee":0})");
            auto measure = [&] (te::Plugin& p, double hz, float amp, int seconds = 1) -> double
            {
                p.baseClassInitialise ({ te::TimePosition(), 48000.0, 256 });
                const int N = 48000 * seconds; juce::AudioBuffer<float> buf (2, 256); te::MidiMessageArray midi; double pk = 0;
                for (int q = 0; q + 256 <= N; q += 256)
                {
                    for (int i = 0; i < 256; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / 48000.0); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                    te::PluginRenderContext fc (&buf, 0, 256, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 48000.0), te::TimePosition::fromSeconds ((q + 256) / 48000.0)), true, false, true, false);
                    p.applyToBuffer (fc);
                    if (q > N - 9600) pk = std::max (pk, (double) buf.getMagnitude (0, 0, 256));
                }
                p.baseClassDeinitialise();
                return 20.0 * std::log10 (pk + 1e-12);
            };
            auto twin = session->edit->getPluginCache().createNewPlugin (SqTorqueCompPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqTorqueCompPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (tq->state);
                const double out = measure (*tw, 1000, 0.5f), want = tw->curveOutDb (-6.02);
                check (std::abs (out - want) < 0.6, "Torque Comp (Clean, -24 dB, 4:1) rendered: a -6 dBFS sine comes out at " + juce::String (out, 2) + " dBFS (its curve says " + juce::String (want, 2) + ")");
                tw->bank.params[(size_t) SqTorqueCompPlugin::lookahead]->setParameter (5.0f, juce::dontSendNotification);
                check (juce::roundToInt (tw->getLatencySeconds() * 48000.0) == 240, "5 ms lookahead reports 240 samples of latency to the engine");
                tw->bank.params[(size_t) SqTorqueCompPlugin::lookahead]->setParameter (0.0f, juce::dontSendNotification);
                tw->bank.params[(size_t) SqTorqueCompPlugin::mix]->setParameter (200.0f, juce::dontSendNotification);
                const double deep = measure (*tw, 1000, 0.5f);
                const double mk = tw->bank[SqTorqueCompPlugin::makeup], gr1 = out + 6.02 - mk, gr2 = deep + 6.02 - mk;   // the makeup isn't gain reduction
                check (std::abs (gr2 - 2.0 * gr1) < 0.8, "Mix 200 %: twice the gain reduction (" + juce::String (gr1, 1) + " -> " + juce::String (gr2, 1) + " dB)");
                // every style renders finite and in range
                bool ok = true; juce::String bad;
                tw->bank.params[(size_t) SqTorqueCompPlugin::mix]->setParameter (100.0f, juce::dontSendNotification);
                for (int sIdx = 0; sIdx < 14; ++sIdx) { tw->style.setValue (sIdx, nullptr); const double o = measure (*tw, 220, 0.7f); if (! std::isfinite (o) || o > 6) { ok = false; bad = SqTorque::styles()[(size_t) sIdx].name; } }
                check (ok, "all 14 styles render a loud tone finite and in range" + (ok ? juce::String() : " - " + bad));
            }
            // MIDI key
            call ("torque_comp", R"({"track":"Comp","action":"set","key":"midi","style":"pump"})");
            check (tq->keySource.get() == 2 && tq->style.get() == 11 && tq->takesMidiInput(), "the MIDI key: note-ons duck the track (Pump style)");
            // key EQ
            call ("torque_comp", R"({"track":"Comp","action":"key_band","band":1,"shape":"low cut","freq":120,"slope":24})");
            check (tq->keyProps[0].on.get() && tq->keyProps[0].shape.get() == 4 && std::abs (tq->bank[SqTorqueCompPlugin::k1Freq] - 120.0f) < 0.5f, "torque_comp key_band: a 120 Hz low cut in the detector");
            // presets
            int nPre = 0, nAdv = 0; for (auto& e : Signature::entries()) if (juce::String (e.type) == "sqmTorqueComp") { nPre = (int) e.presets.size(); for (auto& pr : e.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("torque_comp", R"({"track":"Comp","action":"preset","preset":"Drums | Bus Glue"})");
            check (nPre >= 30 && nAdv >= 10 && pl.contains ("Loaded") && tq->style.get() == 2 && tq->keyProps[0].on.get(), "Torque Comp: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); 'Drums | Bus Glue' loads Glue with its key EQ");
            check (Edition::packOf ("sqmTorqueComp") == Edition::Pack::mastering, "Torque Comp belongs to the Mastering Suite");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = tq->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*tq);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                { juce::FileOutputStream os (juce::File ("/tmp/torque_window.png")); os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (snap, os); }
                check (snap.isValid() && snap.getWidth() >= 1000, "Torque Comp window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Torque section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Summit Limiter
    {
        std::cout << "\n-- 1.14 Studio Line: Summit Limiter --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Summit"})");
        call ("add_track", R"({"kind":"audio","name":"Master Bus"})");
        auto added = call ("summit_limiter", R"({"track":"Master Bus","action":"add"})");
        SqSummitLimiterPlugin* sl = nullptr;
        if (auto tt = session->findTrack ("Master Bus")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqSummitLimiterPlugin*> (pl)) sl = c;
        check (sl != nullptr && added.contains ("Summit Limiter"), "Summit Limiter inserted by summit_limiter add");
        if (sl != nullptr)
        {
            auto st = call ("summit_limiter", R"({"track":"Master Bus","action":"set","algorithm":"aggressive","input":10,"ceiling":-1,"oversampling":4,"dither":16,"noise_shaping":"strong","target":"club"})");
            check (sl->algorithm.get() == 4 && std::abs (sl->bank[SqSummitLimiterPlugin::input] - 10.0f) < 0.01f && sl->oversample.get() == 2 && sl->ditherBits.get() == 1 && sl->noiseShape.get() == 2 && sl->target.get() == 5 && st.contains ("Aggressive"),
                   "summit_limiter set: algorithm, input, 4x, 16-bit strong-shaped dither, the club target - by name");
            call ("summit_limiter", R"({"track":"Master Bus","action":"set","oversampling":1,"dither":0})");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqSummitLimiterPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqSummitLimiterPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (sl->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 3; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; juce::Random rnd (5);
                std::vector<float> outL; outL.reserve ((size_t) N);
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        const double t = (q + i) / 44100.0, beat = std::fmod (t, 0.25);
                        const float v = (float) ((beat < 0.03 ? (1 - beat / 0.03) * (rnd.nextDouble() * 2 - 1) : 0) + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 55 * t));
                        buf.setSample (0, i, v); buf.setSample (1, i, v * 0.8f);
                    }
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    for (int i = 0; i < 512; ++i) outL.push_back (buf.getSample (0, i));
                }
                tw->baseClassDeinitialise();
                // exact band-limited true peak of the last two seconds (FFT, spectrum zero-padded 8x)
                const int M = 65536; juce::dsp::FFT f (16), g (19);
                std::vector<std::complex<float>> a ((size_t) M), A ((size_t) M), B ((size_t) M * 8), b ((size_t) M * 8);
                for (int i = 0; i < M; ++i) a[(size_t) i] = outL[(size_t) (outL.size() - M + i)];
                f.perform (a.data(), A.data(), false);
                for (int k = 0; k < M / 2; ++k) { B[(size_t) k] = A[(size_t) k]; if (k > 0) B[(size_t) (M * 8 - k)] = A[(size_t) (M - k)]; }
                g.perform (B.data(), b.data(), true);
                double tp = 0; for (int i = 4096 * 8; i < (M - 4096) * 8; ++i) tp = std::max (tp, (double) std::abs (b[(size_t) i].real()) * 8);
                const double tpDb = 20 * std::log10 (tp + 1e-12);
                check (tpDb <= -0.9, "Summit (Aggressive, +10 dB, -1 dBTP) rendered: true peak by exact reconstruction " + juce::String (tpDb, 2) + " dBTP");
                check (std::abs (tw->getLatencySeconds() * 44100.0 - (2 * SqSummit::BandGuard::delay + std::lround (0.002 * 44100.0) + SqSummit::TruePeak4::delay + SqSummit::Engine::safetyDelay())) < 1.5,
                       "latency reported: " + juce::String (tw->getLatencySeconds() * 1000.0, 2) + " ms (guards, 2 ms lookahead, detector, safety stage)");
                check (tw->integrated.load() < 0 && tw->momentary.load() > -40, "the loudness panel measures (momentary " + juce::String (tw->momentary.load(), 1) + " LUFS)");
            }
            int nPre = 0, nAdv = 0; for (auto& e : Signature::entries()) if (juce::String (e.type) == "sqmSummitLim") { nPre = (int) e.presets.size(); for (auto& pr : e.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("summit_limiter", R"({"track":"Master Bus","action":"preset","preset":"Export | 16-bit"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && sl->ditherBits.get() == 1 && sl->noiseShape.get() == 2, "Summit Limiter: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the 16-bit export preset sets shaped dither");
            check (Edition::packOf ("sqmSummitLim") == Edition::Pack::mastering, "Summit Limiter belongs to the Mastering Suite");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = sl->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*sl);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Summit Limiter window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Summit section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Atrium Verb
    {
        std::cout << "\n-- 1.14 Studio Line: Atrium Verb --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Atrium"})");
        call ("add_track", R"({"kind":"audio","name":"Verb Return"})");
        auto added = call ("atrium_verb", R"({"track":"Verb Return","action":"add"})");
        SqAtriumVerbPlugin* at = nullptr;
        if (auto tt = session->findTrack ("Verb Return")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqAtriumVerbPlugin*> (pl)) at = c;
        check (at != nullptr && added.contains ("Atrium Verb"), "Atrium Verb inserted by atrium_verb add");
        if (at != nullptr)
        {
            auto st = call ("atrium_verb", R"({"track":"Verb Return","action":"set","space":"cathedral","decay":3,"size":70,"predelay_sync":"1/8","low_decay":1.5,"ducking":6,"low_cut":120,"mix":100})");
            check (at->space.get() == 6 && std::abs (at->bank[SqAtriumVerbPlugin::decay] - 3.0f) < 0.01f && at->predelaySync.get() == 4 && std::abs (at->bank[SqAtriumVerbPlugin::duck] - 6.0f) < 0.01f
                   && st.contains ("Cathedral") && st.contains ("decay_by_frequency_s"), "atrium_verb set: space, decay, synced predelay, low decay, ducking, low cut - by name");
            check (std::abs (at->predelayMs() - 250.0) < 1.0, "predelay synced to 1/8 at 120 BPM = " + juce::String (at->predelayMs(), 1) + " ms");
            call ("atrium_verb", R"({"track":"Verb Return","action":"set","space":"concert hall","decay":2,"predelay":0,"ducking":0,"low_cut":10,"low_decay":1,"high_decay":1,"early":0})");
            check (at->predelaySync.get() == 0, "a predelay in ms sets the sync back to free");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqAtriumVerbPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqAtriumVerbPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (at->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 4; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                std::vector<double> e; e.reserve ((size_t) N); bool finite = true;
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    buf.clear(); if (q == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    for (int i = 0; i < 512; ++i) { const float v = buf.getSample (0, i); if (! std::isfinite (v)) finite = false; e.push_back ((double) v * v); }
                }
                tw->baseClassDeinitialise();
                // broadband Schroeder decay, T20 x 3
                for (int i = (int) e.size() - 2; i >= 0; --i) e[(size_t) i] += e[(size_t) i + 1];
                double t5 = -1, t25 = -1;
                for (size_t i = 0; i < e.size(); ++i) { const double d = 10 * std::log10 (std::max (1e-300, e[i] / e[0])); if (t5 < 0 && d <= -5) t5 = i / 44100.0; if (t25 < 0 && d <= -25) { t25 = i / 44100.0; break; } }
                const double rt = 3 * (t25 - t5);
                check (finite && t25 > 0 && std::abs (rt / 2.0 - 1) < 0.15, "Atrium (Concert Hall, Decay 2 s) rendered: measured RT60 " + juce::String (rt, 2) + " s");
                check (std::abs (tw->decayAt (1000.0) / 2.0 - 1) < 0.05, "the face's decay curve at 1 kHz reads " + juce::String (tw->decayAt (1000.0), 2) + " s");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmAtriumVerb") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("atrium_verb", R"({"track":"Verb Return","action":"preset","preset":"Freeze Pad"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && at->freeze.get() && at->space.get() == 9, "Atrium Verb: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the freeze preset freezes the Bloom space");
            check (Edition::packOf ("sqmAtriumVerb") == Edition::Pack::color, "Atrium Verb belongs to Color & Space");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = at->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*at);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Atrium Verb window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Atrium section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Apex Multiband 2
    {
        std::cout << "\n-- 1.14 Studio Line: Apex Multiband 2 --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest MB2"})");
        call ("add_track", R"({"kind":"audio","name":"Mix Bus"})");
        auto added = call ("apex_multiband", R"({"track":"Mix Bus","action":"add"})");
        SqApexMB2Plugin* mb = nullptr;
        if (auto tt = session->findTrack ("Mix Bus")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqApexMB2Plugin*> (pl)) mb = c;
        check (mb != nullptr && added.contains ("Apex Multiband 2"), "Apex Multiband 2 inserted by apex_multiband add");
        if (mb != nullptr)
        {
            auto st = call ("apex_multiband", R"({"track":"Mix Bus","action":"set","bands":3,"crossovers":[200,5000],"lookahead":2,"detector":"rms"})");
            check (mb->numBands.get() == 3 && std::abs (mb->bank[SqApexMB2Plugin::x2] - 5000.0f) < 1.0f && mb->detector.get() == 1 && st.contains ("crossovers_hz"), "apex_multiband set: bands, crossovers, lookahead, detector");
            auto bd = call ("apex_multiband", R"({"track":"Mix Bus","action":"band","band":2,"mode":"compress","place":"side","threshold":-30,"ratio":4,"range":9,"knee":0,"attack":1,"release":50})");
            check (mb->place[1].get() == 4 && std::abs (mb->bp (1, SqApexMB2Plugin::range) - 9.0f) < 0.01f && bd.contains ("Side"), "apex_multiband band: mode, side-only place, threshold, ratio, range");
            call ("apex_multiband", R"({"track":"Mix Bus","action":"band","band":2,"place":"stereo","range":40})");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqApexMB2Plugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqApexMB2Plugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (mb->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; double eIn = 0, eOut = 0;
                const int lat = (int) std::lround (tw->getLatencySeconds() * 44100.0);
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    for (int i = 0; i < 512; ++i) { const float v = (float) (0.316 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (q + i) / 44100.0)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    if (q > 44100) for (int i = 0; i < 512; ++i) { eOut += (double) buf.getSample (0, i) * buf.getSample (0, i); eIn += 0.316 * 0.316 * 0.5; }
                }
                tw->baseClassDeinitialise();
                const double g = 10 * std::log10 (eOut / eIn);
                check (std::abs (g + 15.0) < 1.0, "Apex Multiband 2 rendered: band 2 (4:1 at -30 dB) takes a -10 dB tone down " + juce::String (g, 2) + " dB (curve -15.0)");
                check (lat == (int) std::lround (0.002 * 44100.0), "latency = the 2 ms lookahead (" + juce::String (lat) + " samples)");
            }
            call ("apex_multiband", R"({"track":"Mix Bus","action":"set","phase":"linear"})");
            check (std::abs (mb->getLatencySeconds() * mb->sampleRate() - SqMB2::Engine::linearLatency (mb->sampleRate())) < 1.5, "linear phase reports its FIR latency (" + juce::String (mb->getLatencySeconds() * 1000.0, 1) + " ms)");
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmApexMB2") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("apex_multiband", R"({"track":"Mix Bus","action":"preset","preset":"Kick Ducks"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && mb->keyOf[0].get() == 1, "Apex Multiband 2: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the side-chain preset keys band 1");
            check (Edition::packOf ("sqmApexMB2") == Edition::Pack::mastering, "Apex Multiband 2 belongs to the Mastering Suite");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = mb->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*mb);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Apex Multiband 2 window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 MB2 section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Sibyl De-Ess
    {
        std::cout << "\n-- 1.14 Studio Line: Sibyl De-Ess --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Sibyl"})");
        call ("add_track", R"({"kind":"audio","name":"Lead Vox"})");
        auto added = call ("sibyl_deess", R"({"track":"Lead Vox","action":"add"})");
        SqSibylDeEssPlugin* sy = nullptr;
        if (auto tt = session->findTrack ("Lead Vox")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqSibylDeEssPlugin*> (pl)) sy = c;
        check (sy != nullptr && added.contains ("Sibyl De-Ess"), "Sibyl De-Ess inserted by sibyl_deess add");
        if (sy != nullptr)
        {
            auto st = call ("sibyl_deess", R"({"track":"Lead Vox","action":"set","detection":"absolute","threshold":-40,"range":10,"knee":0,"lookahead":0,"low":5000,"high":10000,"follow":false,"stereo":"side"})");
            check (sy->detect.get() == 0 && sy->stereoMode.get() == 2 && std::abs (sy->bank[SqSibylDeEssPlugin::range] - 10.0f) < 0.01f && st.contains ("Absolute"), "sibyl_deess set: detection, threshold, range, band, stereo - by name");
            call ("sibyl_deess", R"({"track":"Lead Vox","action":"set","stereo":"linked"})");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqSibylDeEssPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqSibylDeEssPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (sy->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; double eIn = 0, eOut = 0, lowIn = 0, lowOut = 0;
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        const double t = (q + i) / 44100.0;
                        const float v = (float) (0.2 * std::sin (juce::MathConstants<double>::twoPi * 7000.0 * t) + 0.2 * std::sin (juce::MathConstants<double>::twoPi * 200.0 * t));
                        buf.setSample (0, i, v); buf.setSample (1, i, v);
                    }
                    juce::AudioBuffer<float> in (buf);
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    if (q > 44100)
                        for (int i = 0; i < 512; ++i)
                        {
                            const double t = (q + i) / 44100.0, s7 = std::sin (juce::MathConstants<double>::twoPi * 7000.0 * t), c7 = std::cos (juce::MathConstants<double>::twoPi * 7000.0 * t);
                            const double s2 = std::sin (juce::MathConstants<double>::twoPi * 200.0 * t), c2 = std::cos (juce::MathConstants<double>::twoPi * 200.0 * t);
                            eIn += in.getSample (0, i) * s7; eOut += buf.getSample (0, i) * s7; lowIn += in.getSample (0, i) * s2; lowOut += buf.getSample (0, i) * s2;
                            juce::ignoreUnused (c7, c2);
                        }
                }
                tw->baseClassDeinitialise();
                const double d7 = 20 * std::log10 (std::abs (eOut) / std::abs (eIn) + 1e-12), d2 = 20 * std::log10 (std::abs (lowOut) / std::abs (lowIn) + 1e-12);
                check (d7 < -7.0 && std::abs (d2) < 0.3, "Sibyl rendered (split, range 10): the 7 kHz s " + juce::String (d7, 2) + " dB, the 200 Hz body " + juce::String (d2, 2) + " dB");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmSibylDeEss") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("sibyl_deess", R"({"track":"Lead Vox","action":"preset","preset":"Hunt the S"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && sy->track.get(), "Sibyl De-Ess: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the hunting preset turns Track on");
            check (Edition::packOf ("sqmSibylDeEss") == Edition::Pack::vocal, "Sibyl De-Ess belongs to the Vocal Suite");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = sy->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*sy);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Sibyl De-Ess window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Sibyl section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Portcullis Gate
    {
        std::cout << "\n-- 1.14 Studio Line: Portcullis Gate --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Portcullis"})");
        call ("add_track", R"({"kind":"audio","name":"Snare"})");
        auto added = call ("portcullis_gate", R"({"track":"Snare","action":"add"})");
        SqPortcullisGatePlugin* pgt = nullptr;
        if (auto tt = session->findTrack ("Snare")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqPortcullisGatePlugin*> (pl)) pgt = c;
        check (pgt != nullptr && added.contains ("Portcullis Gate"), "Portcullis Gate inserted by portcullis_gate add");
        if (pgt != nullptr)
        {
            auto st = call ("portcullis_gate", R"({"track":"Snare","action":"set","mode":"gate","threshold":-30,"hysteresis":6,"range":40,"hold":20,"release":50,"key_highpass":150,"curve":"punchy"})");
            check (std::abs (pgt->bank[SqPortcullisGatePlugin::range] - 40.0f) < 0.01f && pgt->curve.get() == 2 && st.contains ("close_threshold_db"), "portcullis_gate set: thresholds, range, hold, release, key filter, curve - by name");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqPortcullisGatePlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqPortcullisGatePlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (pgt->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; double eIn = 0, eOut = 0;
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    for (int i = 0; i < 512; ++i) { const float v = (float) (0.003 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * (q + i) / 44100.0)); buf.setSample (0, i, v); buf.setSample (1, i, v); }   // -50 dB: below the gate
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    if (q > 44100) for (int i = 0; i < 512; ++i) { eOut += (double) buf.getSample (0, i) * buf.getSample (0, i); eIn += 0.003 * 0.003 * 0.5; }
                }
                tw->baseClassDeinitialise();
                const double g = 10 * std::log10 (eOut / eIn + 1e-30);
                check (std::abs (g + 40.0) < 0.2, "Portcullis rendered: a -50 dB tone under a -30 dB gate is held " + juce::String (g, 2) + " dB down (range 40)");
            }
            call ("portcullis_gate", R"({"track":"Snare","action":"set","lookahead":3})");
            check (std::abs (pgt->getLatencySeconds() * 1000.0 - 3.0) < 0.05, "lookahead reported as latency (" + juce::String (pgt->getLatencySeconds() * 1000.0, 2) + " ms)");
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmPortcullis") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("portcullis_gate", R"({"track":"Snare","action":"preset","preset":"Trance Gate"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && pgt->keySource.get() == 2, "Portcullis Gate: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the trance-gate preset keys from MIDI");
            check (Edition::packOf ("sqmPortcullis") == Edition::Pack::mastering, "Portcullis Gate belongs to the Mastering Suite");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = pgt->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*pgt);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Portcullis Gate window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Portcullis section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Kiln
    {
        std::cout << "\n-- 1.14 Studio Line: Kiln --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Kiln"})");
        call ("add_track", R"({"kind":"audio","name":"Drum Bus"})");
        auto added = call ("kiln", R"({"track":"Drum Bus","action":"add"})");
        SqKilnPlugin* kl = nullptr;
        if (auto tt = session->findTrack ("Drum Bus")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqKilnPlugin*> (pl)) kl = c;
        check (kl != nullptr && added.contains ("Kiln"), "Kiln inserted by kiln add");
        if (kl != nullptr)
        {
            call ("kiln", R"({"track":"Drum Bus","action":"set","bands":2,"crossovers":[300],"oversampling":4,"auto_gain":false})");
            auto bd = call ("kiln", R"({"track":"Drum Bus","action":"band","band":1,"model":"pentode box","drive":24,"mix":100})");
            call ("kiln", R"({"track":"Drum Bus","action":"band","band":2,"drive":-12,"mix":0})");
            auto md = call ("kiln", R"({"track":"Drum Bus","action":"mod","slot":1,"source":"lfo","target":"band 1 drive","amount":6})");
            check (kl->numBands.get() == 2 && kl->model[0].get() == Cinder::pentodeBox && kl->slotTarget[0].get() == 1 && std::abs (kl->bank[SqKilnPlugin::slot1] * 36.0f - 6.0f) < 0.01f && md.contains ("Band 1 Drive"),
                   "kiln set / band / mod: two bands, a pentode low band, an LFO on its drive - by name");
            call ("kiln", R"({"track":"Drum Bus","action":"mod","slot":1,"source":"off"})");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqKilnPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqKilnPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (kl->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100 * 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; std::vector<float> out;
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    for (int i = 0; i < 512; ++i) { const double t = (q + i) / 44100.0; const float v = (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * 100.0 * t) + 0.1 * std::sin (juce::MathConstants<double>::twoPi * 3050.0 * t)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
                }
                tw->baseClassDeinitialise();
                auto bin = [&] (double f) { std::complex<double> acc = 0; double ws = 0; const int from = 44100, M = 16384;
                    for (int i = 0; i < M; ++i) { const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (M - 1)); acc += w * (double) out[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / 44100.0); ws += w; }
                    return 20 * std::log10 (2 * std::abs (acc) / ws + 1e-12); };
                const double h3 = bin (300) - bin (100), tone = bin (3050), side = std::max (bin (2950), bin (3150));
                check (h3 > -30 && std::abs (tone - 20 * std::log10 (0.1)) < 0.5 && side < tone - 50,
                       "Kiln rendered: the driven low band adds a 3rd harmonic at " + juce::String (h3, 1) + " dBc; the clean upper band keeps 3.05 kHz at " + juce::String (tone, 2) + " dB, no intermodulation");
            }
            check (kl->getLatencySeconds() > 0.0, "4x oversampling reports its latency (" + juce::String (kl->getLatencySeconds() * 1000.0, 2) + " ms)");
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmKiln") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("kiln", R"({"track":"Drum Bus","action":"preset","preset":"Louder Is Dirtier"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && kl->slotSource[0].get() == 2, "Kiln: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the dynamic preset routes the envelope");
            check (Edition::packOf ("sqmKiln") == Edition::Pack::color, "Kiln belongs to Color & Space");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = kl->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*kl);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Kiln window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Kiln section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Spindle Delay
    {
        std::cout << "\n-- 1.14 Studio Line: Spindle Delay --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Spindle"})");
        call ("add_track", R"({"kind":"audio","name":"Echo Return"})");
        auto added = call ("spindle_delay", R"({"track":"Echo Return","action":"add"})");
        SqSpindleDelayPlugin* sd = nullptr;
        if (auto tt = session->findTrack ("Echo Return")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqSpindleDelayPlugin*> (pl)) sd = c;
        check (sd != nullptr && added.contains ("Spindle Delay"), "Spindle Delay inserted by spindle_delay add");
        if (sd != nullptr)
        {
            auto st = call ("spindle_delay", R"({"track":"Echo Return","action":"set","character":"digital","routing":"pingpong","sync":"1/8 dotted","feedback":50,"mix":100})");
            check (sd->character.get() == 0 && sd->routing.get() == 1 && sd->syncL.get() == 8 && std::abs (sd->bank[SqSpindleDelayPlugin::wow]) < 0.01f && st.contains ("Ping-Pong"),
                   "spindle_delay set: character (with its own wow), ping-pong, a dotted-eighth sync - by name");
            check (std::abs (sd->timeMs (0) - 375.0) < 0.5, "1/8 dotted at 120 BPM = " + juce::String (sd->timeMs (0), 1) + " ms");
            call ("spindle_delay", R"({"track":"Echo Return","action":"set","routing":"normal","time":200,"low_cut":10,"high_cut":20000,"drive":0})");
            check (sd->syncL.get() == 0, "a time in ms sets the sync back to free");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqSpindleDelayPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqSpindleDelayPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (sd->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                const int N = 44100; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; std::vector<float> out;
                for (int q = 0; q + 512 <= N; q += 512)
                {
                    buf.clear(); if (q == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
                    te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                    tw->applyToBuffer (fc);
                    for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
                }
                tw->baseClassDeinitialise();
                auto peakNear = [&] (int at) { int best = at; for (int i = at - 50; i <= at + 50; ++i) if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) best])) best = i; return best; };
                const int e1 = peakNear (8820), e2 = peakNear (17640);
                const double ratio = 20 * std::log10 (std::abs (out[(size_t) e2]) / std::max (1e-9f, std::abs (out[(size_t) e1])));
                check (std::abs (e1 - 8820) <= 1 && std::abs (e2 - 17640) <= 1 && std::abs (ratio - 20 * std::log10 (0.5)) < 0.2,
                       "Spindle rendered: echoes at 200 / 400 ms (samples " + juce::String (e1) + ", " + juce::String (e2) + "), the second " + juce::String (ratio, 2) + " dB (feedback 50 %)");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmSpindle") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("spindle_delay", R"({"track":"Echo Return","action":"preset","preset":"Freeze Pad"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && sd->freeze.get(), "Spindle Delay: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the freeze preset freezes");
            check (Edition::packOf ("sqmSpindle") == Edition::Pack::color, "Spindle Delay belongs to Color & Space");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = sd->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*sd);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Spindle Delay window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Spindle section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": Fissure Filter
    {
        std::cout << "\n-- 1.14 Studio Line: Fissure Filter --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Fissure"})");
        call ("add_track", R"({"kind":"audio","name":"Synth"})");
        auto added = call ("fissure_filter", R"({"track":"Synth","action":"add"})");
        SqFissureFilterPlugin* fi = nullptr;
        if (auto tt = session->findTrack ("Synth")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqFissureFilterPlugin*> (pl)) fi = c;
        check (fi != nullptr && added.contains ("Fissure Filter"), "Fissure Filter inserted by fissure_filter add");
        if (fi != nullptr)
        {
            call ("fissure_filter", R"({"track":"Synth","action":"filter","filter":1,"type":"low-pass","slope":24,"cutoff":1000,"resonance":0})");
            auto md = call ("fissure_filter", R"({"track":"Synth","action":"mod","lfo1_shape":"square","lfo1_sync":"1/4","slot":1,"source":"lfo 1","target":"filter 1 cutoff","amount":2})");
            check (fi->slotType[0].get() == SqFissure::lowPass && fi->lfo1Sync.get() == 5 && fi->modDst[0].get() == 1 && std::abs (fi->bank[SqFissureFilterPlugin::mod1] * 6.0f - 2.0f) < 0.01f && md.contains ("Filter 1 Cutoff"),
                   "fissure_filter filter / mod: a 24 dB low-pass, a synced square LFO on its cutoff - by name");
            check (std::abs (fi->lfoRate (0) - 2.0) < 0.01, "LFO synced to 1/4 at 120 BPM = " + juce::String (fi->lfoRate (0), 2) + " Hz");
            call ("fissure_filter", R"({"track":"Synth","action":"mod","slot":1,"source":"off"})");
            auto twin = session->edit->getPluginCache().createNewPlugin (SqFissureFilterPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqFissureFilterPlugin*> (twin.get()))
            {
                tw->restorePluginStateFromValueTree (fi->state);
                tw->baseClassInitialise ({ te::TimePosition(), 44100.0, 512 });
                auto level = [&] (double f)
                {
                    tw->reset(); const int N = 44100 / 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; double eo = 0, ei = 0;
                    for (int q = 0; q + 512 <= N; q += 512)
                    {
                        for (int i = 0; i < 512; ++i) { const float v = (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * f * (q + i) / 44100.0)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                        te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                        tw->applyToBuffer (fc);
                        if (q > N / 2) for (int i = 0; i < 512; ++i) { eo += (double) buf.getSample (0, i) * buf.getSample (0, i); ei += 0.25 * 0.25 * 0.5; }
                    }
                    return 10 * std::log10 (eo / ei + 1e-30);
                };
                const double atFc = level (1000), at4k = level (4000);
                tw->baseClassDeinitialise();
                check (std::abs (atFc + 3.0) < 0.3 && at4k < -45, "Fissure rendered: 24 dB low-pass at 1 kHz reads " + juce::String (atFc, 2) + " dB at the cutoff, " + juce::String (at4k, 1) + " dB at 4 kHz");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmFissure") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("fissure_filter", R"({"track":"Synth","action":"preset","preset":"Auto-Wah"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && fi->modSrc[0].get() == 3, "Fissure Filter: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the auto-wah routes the envelope");
            check (Edition::packOf ("sqmFissure") == Edition::Pack::color, "Fissure Filter belongs to Color & Space");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = fi->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*fi);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Fissure Filter window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Fissure section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": phase K small tools - Mote, Duet Filter, Speck Filter
    {
        std::cout << "\n-- 1.14 Studio Line: Mote, Duet Filter, Speck Filter --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Small Tools"})");
        call ("add_track", R"({"kind":"instrument","name":"MoteTrack"})");
        auto added = call ("mote", R"({"track":"MoteTrack","action":"add"})");
        SqMotePlugin* mo = nullptr;
        if (auto tt = session->findTrack ("MoteTrack")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqMotePlugin*> (pl)) mo = c;
        check (mo != nullptr && added.contains ("\"Mote\""), "Mote inserted by mote add");
        if (mo != nullptr)
        {
            call ("mote", R"({"track":"MoteTrack","action":"set","params":{"shape":0,"cutoff":20000,"res":0,"fEnvDepth":0,"keyTrack":0,"aA":1,"aS":100,"level":0},"filter_type":"low-pass","slope":12,"voice":"mono","quality":"eco"})");
            auto md = call ("mote", R"({"track":"MoteTrack","action":"mod","slot":1,"source":"mod wheel","target":"cutoff","amount":0.5})");
            check (mo->polyModeValue().get() == 1 && mo->qualityValue().get() == 0 && mo->modDestValue (0).get() == "cutoff" && mo->modSourceValue (0).get() == (int) SqSynth::ModSource::modWheel && md.contains ("Mod Wheel"),
                   "mote set / mod: mono, Eco, a sine, mod wheel to cutoff - by name");
            call ("mote", R"({"track":"MoteTrack","action":"mod","slot":1,"source":"off","target":"none","amount":0})");
            // rendered: A4 is 440 Hz, and the attack is what it says
            auto pp = engine->getPluginManager().createNewPlugin (*session->edit, SqMotePlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqMotePlugin*> (pp.get()))
            {
                tw->restorePluginStateFromValueTree (mo->state);
                const double SRm = 48000.0; const int block = 256; pp->baseClassInitialise ({ te::TimePosition(), SRm, block });
                juce::AudioBuffer<float> buf (2, block); te::MidiMessageArray midi; std::vector<float> out;
                for (int q = 0; q < (int) SRm; q += block)
                {
                    midi.clear(); if (q == 0) midi.addMidiMessage (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 127), 0.0, 0);
                    te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / SRm), te::TimePosition::fromSeconds ((q + block) / SRm)), true, false, true, false);
                    pp->applyToBuffer (fc);
                    for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
                }
                pp->baseClassDeinitialise();
                double fa = 0, fb = 0; int nc = 0;
                for (size_t i = 4800; i < out.size(); ++i) if (out[i - 1] < 0 && out[i] >= 0) { const double t = (double) i - 1 + out[i - 1] / (out[i - 1] - out[i]); if (nc == 0) fa = t; fb = t; ++nc; }
                const double hz = nc > 1 ? (nc - 1) / ((fb - fa) / SRm) : 0;
                double pk = 0; for (size_t i = 4800; i < out.size(); ++i) pk = std::max (pk, (double) std::abs (out[i]));
                check (std::abs (1200 * std::log2 (std::max (1.0, hz) / 440.0)) < 0.5 && pk > 0.05 && pk < 1.0, "Mote rendered: A4 plays " + juce::String (hz, 3) + " Hz, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmMote") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("mote", R"({"track":"MoteTrack","action":"preset","preset":"Squelch Acid"})");
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && mo->polyModeValue().get() == 2, "Mote: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the acid bass plays legato");
            check (Edition::packOf ("sqmMote") == Edition::Pack::none, "Mote is part of the core program");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = mo->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*mo);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Mote window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }

        call ("add_track", R"({"kind":"audio","name":"Fx"})");
        auto renderLevel = [&] (te::Plugin& p, double f)
        {
            p.baseClassInitialise ({ te::TimePosition(), 44100.0, 512 }); p.reset();
            const int N = 44100 / 2; juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi; double eo = 0, ei = 0;
            for (int q = 0; q + 512 <= N; q += 512)
            {
                for (int i = 0; i < 512; ++i) { const float v = (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * f * (q + i) / 44100.0)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / 44100.0), te::TimePosition::fromSeconds ((q + 512) / 44100.0)), true, false, true, false);
                p.applyToBuffer (fc);
                if (q > N / 2) for (int i = 0; i < 512; ++i) { eo += (double) buf.getSample (0, i) * buf.getSample (0, i); ei += 0.25 * 0.25 * 0.5; }
            }
            p.baseClassDeinitialise();
            return 10 * std::log10 (eo / ei + 1e-30);
        };
        auto dAdd = call ("duet_filter", R"({"track":"Fx","action":"add"})");
        SqDuetFilterPlugin* du = nullptr;
        if (auto tt = session->findTrack ("Fx")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqDuetFilterPlugin*> (pl)) du = c;
        check (du != nullptr && dAdd.contains ("Duet Filter"), "Duet Filter inserted by duet_filter add");
        if (du != nullptr)
        {
            call ("duet_filter", R"({"track":"Fx","action":"filter","filter":1,"type":"low-pass","slope":24,"cutoff":1000,"resonance":0})");
            call ("duet_filter", R"({"track":"Fx","action":"filter","filter":2,"type":"high-pass","slope":24,"resonance":0})");
            auto st = call ("duet_filter", R"({"track":"Fx","action":"set","routing":"serial","link_interval":0,"lfo_sync":"1/8"})");
            check (du->link.get() && du->type[1].get() == SqFissure::highPass && std::abs (du->snapshot().baseCut (1) - 1000.0) < 0.5 && du->lfoSync.get() == 6 && st.contains ("Serial"),
                   "duet_filter: low-pass 1k into a high-pass linked at 0 octaves, LFO synced to 1/8 - by name");
            auto pp = engine->getPluginManager().createNewPlugin (*session->edit, SqDuetFilterPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqDuetFilterPlugin*> (pp.get()))
            {
                tw->restorePluginStateFromValueTree (du->state);
                const double at1k = renderLevel (*tw, 1000), at100 = renderLevel (*tw, 100);
                check (std::abs (at1k + 6.0) < 0.4 && at100 < -45, "Duet rendered: LP 1k -> HP 1k reads " + juce::String (at1k, 2) + " dB at 1 kHz, " + juce::String (at100, 1) + " dB at 100 Hz");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmDuetFilter") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("duet_filter", R"({"track":"Fx","action":"preset","preset":"Stereo Swirl"})");
            check (nPre >= 24 && nAdv >= 6 && pl.contains ("Loaded") && du->routing.get() == 2, "Duet Filter: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); Stereo Swirl splits left and right");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = du->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*du);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1000, "Duet Filter window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        auto sAdd = call ("speck_filter", R"({"track":"Fx","action":"add"})");
        SqSpeckFilterPlugin* sk = nullptr;
        if (auto tt = session->findTrack ("Fx")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqSpeckFilterPlugin*> (pl)) sk = c;
        check (sk != nullptr && sAdd.contains ("Speck Filter"), "Speck Filter inserted by speck_filter add");
        if (sk != nullptr)
        {
            auto st = call ("speck_filter", R"({"track":"Fx","action":"set","type":"band-pass","slope":12,"cutoff":2000,"resonance":30,"env_source":"sidechain","lfo_shape":"square"})");
            check (sk->type.get() == SqFissure::bandPass && sk->envKey.get() && sk->lfoShape.get() == 3 && st.contains ("sidechain"), "speck_filter set: a band-pass at 2 kHz, envelope from the sidechain, square LFO - by name");
            auto pp = engine->getPluginManager().createNewPlugin (*session->edit, SqSpeckFilterPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqSpeckFilterPlugin*> (pp.get()))
            {
                tw->restorePluginStateFromValueTree (sk->state);
                const double atC = renderLevel (*tw, 2000), off = renderLevel (*tw, 200);
                check (std::abs (atC) < 0.3 && off < -10, "Speck rendered: band-pass centre " + juce::String (atC, 2) + " dB (unity), " + juce::String (off, 1) + " dB at 200 Hz");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmSpeckFilter") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("speck_filter", R"({"track":"Fx","action":"preset","preset":"Auto-Wah"})");
            check (nPre >= 24 && nAdv >= 5 && pl.contains ("Loaded") && sk->bank[SqSpeckFilterPlugin::envDepth] > 2.0f, "Speck Filter: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); the auto-wah opens on the envelope");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = sk->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*sk);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 900, "Speck Filter window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 small tools section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.14 "Studio Line": phase K - Mirrorwing (the flagship synth)
    {
        std::cout << "\n-- 1.14 Studio Line: Mirrorwing --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Mirrorwing"})");
        call ("add_track", R"({"kind":"instrument","name":"Wing"})");
        auto added = call ("mirrorwing", R"({"track":"Wing","action":"add"})");
        SqMirrorwingPlugin* mw = nullptr;
        if (auto tt = session->findTrack ("Wing")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqMirrorwingPlugin*> (pl)) mw = c;
        check (mw != nullptr && added.contains ("\"Mirrorwing\""), "Mirrorwing inserted by mirrorwing add");
        if (mw != nullptr)
        {
            call ("mirrorwing", R"({"track":"Wing","action":"osc","osc":1,"shape":"sine","level":100,"unison":1})");
            call ("mirrorwing", R"({"track":"Wing","action":"osc","osc":2,"on":false})");
            call ("mirrorwing", R"({"track":"Wing","action":"filter","filter":1,"on":false})");
            call ("mirrorwing", R"({"track":"Wing","action":"env","env":1,"attack":1,"sustain":100})");
            auto st = call ("mirrorwing", R"({"track":"Wing","action":"set","voice":"mono","quality":"eco","params":{"level":0}})");
            auto md = call ("mirrorwing", R"({"track":"Wing","action":"mod","source":"lfo 2","target":"filter 1 cutoff","amount_units":2})");
            const int s0 = [&] { for (int s = 0; s < 16; ++s) if (mw->modSrc[(size_t) s].get() == SqMirror::sLfo2) return s; return -1; }();
            check (mw->polyModeValue().get() == 1 && s0 >= 0 && mw->modDst[(size_t) s0].get() == SqMirror::tFilBase && std::abs (mw->theBank()[(size_t) (SqMirrorwingPlugin::modBase + s0)] * 6.0f - 2.0f) < 0.01f && md.contains ("Filter 1 Cutoff"),
                   "mirrorwing osc / filter / env / set / mod: a sine, mono, LFO 2 to filter 1 cutoff at +2 oct - by name");
            call ("mirrorwing", R"({"track":"Wing","action":"mod","source":"off","target":"none","amount":0,"slot":1})");
            auto pp = engine->getPluginManager().createNewPlugin (*session->edit, SqMirrorwingPlugin::xmlTypeName, {});
            if (auto* tw = dynamic_cast<SqMirrorwingPlugin*> (pp.get()))
            {
                tw->restorePluginStateFromValueTree (mw->state);
                const double SRm = 48000.0; const int block = 256; pp->baseClassInitialise ({ te::TimePosition(), SRm, block });
                juce::AudioBuffer<float> buf (2, block); te::MidiMessageArray midi; std::vector<float> out;
                for (int q = 0; q < (int) SRm; q += block)
                {
                    midi.clear(); if (q == 0) midi.addMidiMessage (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 127), 0.0, 0);
                    te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / SRm), te::TimePosition::fromSeconds ((q + block) / SRm)), true, false, true, false);
                    pp->applyToBuffer (fc);
                    for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
                }
                pp->baseClassDeinitialise();
                double fa = 0, fb = 0; int nc = 0;
                for (size_t i = 4800; i < out.size(); ++i) if (out[i - 1] < 0 && out[i] >= 0) { const double t = (double) i - 1 + out[i - 1] / (out[i - 1] - out[i]); if (nc == 0) fa = t; fb = t; ++nc; }
                const double hz = nc > 1 ? (nc - 1) / ((fb - fa) / SRm) : 0;
                double pk = 0; for (size_t i = 4800; i < out.size(); ++i) pk = std::max (pk, (double) std::abs (out[i]));
                check (std::abs (1200 * std::log2 (std::max (1.0, hz) / 220.0)) < 0.5 && pk > 0.05 && pk < 1.0, "Mirrorwing rendered: A3 plays " + juce::String (hz, 3) + " Hz, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
            }
            int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmMirrorwing") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
            auto pl = call ("mirrorwing", R"({"track":"Wing","action":"preset","preset":"Mirror Glass"})");
            int used = 0; for (int s = 0; s < 16; ++s) used += mw->modSrc[(size_t) s].get() != 0 ? 1 : 0;
            check (nPre >= 24 && nAdv >= 8 && pl.contains ("Loaded") && mw->oscUnison[0].get() == 5 && used == 3, "Mirrorwing: " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " Advanced); Mirror Glass loads 5-voice unison and 3 matrix routings");
            check (Edition::packOf ("sqmMirrorwing") == Edition::Pack::none, "Mirrorwing is part of the core program");
            {
                std::unique_ptr<te::Plugin::EditorComponent> ed = mw->createEditor();
                if (ed == nullptr) ed = std::make_unique<Skins::SkinEditor> (*mw);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
                auto snap = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                check (snap.isValid() && snap.getWidth() >= 1400, "Mirrorwing window renders offscreen (" + juce::String (snap.getWidth()) + " x " + juce::String (snap.getHeight()) + ")");
            }
        }
        std::cout << "  (1.14 Mirrorwing section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== 1.15 Beat Intelligence L1: the Reference Listener
    {
        std::cout << "\n-- 1.15 Reference Listener (analyze_reference) --\n";
        const double startedAt = juce::Time::getMillisecondCounterHiRes();
        call ("new_session", R"({"name":"Selftest Reference"})");
        // a 24-bar 90 BPM beat in D minor: Dm Bb F C pads, kick 1 + "and" of 2 + 3, snare 2 and 4, 8th hats, 808 on the roots;
        // 8-bar intro (pad + hats), then 16 bars with everything
        const double sr = 44100, bpm = 90, beat = 60.0 / bpm, bar = 4 * beat; const int bars = 24;
        std::vector<float> L ((size_t) (sr * (bars * bar + 1.0)), 0.0f), R (L.size(), 0.0f);
        const double tp = juce::MathConstants<double>::twoPi;
        auto mtof = [] (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); };
        juce::Random rnd (7);
        auto hit = [&] (double t0, int kind)
        {
            const size_t a = (size_t) (t0 * sr); const size_t n = (size_t) (sr * (kind == 0 ? 0.35 : kind == 1 ? 0.2 : 0.05));
            double ph = 0;
            for (size_t i = 0; i < n && a + i < L.size(); ++i)
            {
                const double t = i / sr; float v = 0;
                if (kind == 0) { ph += tp * (50 + 120 * std::exp (-t * 40)) / sr; v = (float) (0.8 * std::sin (ph) * std::exp (-t * 9)); }
                else if (kind == 1) v = (float) ((rnd.nextFloat() * 2 - 1) * 0.45 * std::exp (-t * 22) + 0.3 * std::sin (tp * 190 * t) * std::exp (-t * 30));
                else { v = (rnd.nextFloat() * 2 - 1) * 0.18f * (float) std::exp (-t * 90); }
                L[a + i] += v; R[a + i] += v;
            }
        };
        const int roots[4] = { 62, 58, 53, 60 }; const int thirds[4] = { 3, 4, 4, 4 };
        for (int b = 0; b < bars; ++b)
        {
            const int ci = b % 4; const bool intro = b < 8; const double t0 = b * bar;
            const size_t a = (size_t) (t0 * sr), n = (size_t) (bar * sr);
            for (int tone : { 0, thirds[ci], 7 })
                for (size_t i = 0; i < n && a + i < L.size(); ++i) { const double t = i / sr; const float v = (float) (0.045 * std::sin (tp * mtof (roots[ci] + tone) * t) * juce::jmin (1.0, t * 30)); L[a + i] += v; R[a + i] += v * 0.9f; }
            for (int st = 0; st < 16; st += 2) hit (t0 + st * bar / 16, 2);
            if (intro) continue;
            for (int st : { 0, 6, 8 }) hit (t0 + st * bar / 16, 0);
            for (int st : { 4, 12 }) hit (t0 + st * bar / 16, 1);
            double ph = 0;
            for (size_t i = 0; i < (size_t) (n * 0.9) && a + i < L.size(); ++i) { const double t = i / sr; ph += tp * mtof (roots[ci] - 24) / sr; const float v = (float) (0.45 * std::tanh (1.4 * std::sin (ph)) * std::exp (-t * 0.7) * juce::jmin (1.0, t * 300)); L[a + i] += v; R[a + i] += v; }
        }
        auto refF = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_selftest_reference.wav");
        { refF.deleteFile(); juce::AudioBuffer<float> b (2, (int) L.size()); b.copyFrom (0, 0, L.data(), (int) L.size()); b.copyFrom (1, 0, R.data(), (int) R.size()); b.applyGain (0.7f); juce::WavAudioFormat wav;
          if (auto os = refF.createOutputStream()) if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
        call ("add_track", R"({"kind":"audio","name":"Ref Beat"})");
        call ("import_audio", juce::String (R"({"track":"Ref Beat","path":")") + refF.getFullPathName().replace ("\\", "/") + R"(","start_bar":1})");
        const auto rs = call ("analyze_reference", R"({"track":"Ref Beat"})");
        const auto js = juce::JSON::parse (rs.fromFirstOccurrenceOf ("BLUEPRINT JSON:", false, false));
        const double gotBpm = js["bpm"];
        check (std::abs (gotBpm - 90.0) < 1.0 && js["key"].toString() == "D minor",
               "analyze_reference from a track: " + juce::String (gotBpm, 1) + " BPM, " + js["key"].toString() + " (written 90 BPM, D minor)");
        const auto secs = js["sections"];
        bool introFirst = secs.isArray() && secs.size() >= 2 && (int) secs[1]["start_bar"] >= 8 && (int) secs[1]["start_bar"] <= 10;
        check (introFirst, "analyze_reference: the 8-bar intro is its own section (" + juce::String (secs.isArray() ? secs.size() : 0) + " sections, the 2nd at bar "
               + (secs.isArray() && secs.size() > 1 ? secs[1]["start_bar"].toString() : juce::String ("-")) + ")");
        juce::String snareRow = secs.isArray() && secs.size() > 1 ? secs[1]["snare"].toString() : juce::String();
        check (snareRow == "....x.......x...", "analyze_reference: snare on 2 and 4 only, the 8th hats kept out (" + snareRow + ")");
        const auto kickRow = secs.isArray() && secs.size() > 1 ? secs[1]["kick"].toString() : juce::String();
        check (kickRow == "x.....x.x.......", "analyze_reference: kick x.....x.x....... under a held 808 (" + kickRow + ")");
        check ((int) js["808"]["glides"] == 0 && (double) js["mix"]["pump_db"] < 2.0, "analyze_reference: re-struck 808 notes are not glides, no pump without a sidechain ("
               + js["808"]["glides"].toString() + " glides, " + js["mix"]["pump_db"].toString() + " dB)");
        check (rs.contains ("REBUILD PLAN") && rs.contains ("set_tempo bpm 90"), "analyze_reference returns a rebuild plan in Squelchmoth's tools");
        const auto cmp = call ("analyze_reference", juce::String (R"({"action":"compare","against_file":")") + refF.getFullPathName().replace ("\\", "/") + R"("})");
        check (cmp.contains ("COMPARE") && cmp.contains ("D minor vs D minor"), "analyze_reference compare: the reference against itself matches (key D minor vs D minor)");
        const auto pl = call ("analyze_reference", R"({"action":"plan"})");
        check (pl.contains ("808") && pl.contains ("Drums"), "analyze_reference plan: the last reference's rebuild plan");
        check (ProducerBrain::guide().contains ("analyze_reference") && ProducerBrain::guide().contains ("Style cards"), "The producer guide (both assistants) carries the reference procedure and the style cards");
        // ---- the Beat Architect: a whole beat from the reference's feel, then heard back by the Listener
        {
            const auto cards = call ("style_card", R"({"action":"list"})");
            const auto drill = juce::JSON::parse (call ("style_card", R"({"style":"dark drill"})"));
            check (cards.contains ("Modern Trap") && cards.contains ("Neo-Soul") && drill["name"].toString() == "Drill" && drill["hats"].toString().contains ("3+3+2"),
                   "style_card: 14 cards listed; 'dark drill' finds Drill with its 3+3+2 hats");
            const auto pv = call ("build_beat", R"({"style":"trap","key":"A","minor":true,"bpm":144,"seed":3,"preview":true})");
            check (pv.contains ("144 BPM, A minor") && pv.contains ("Hook 16") && session->findTrack ("Chords") == nullptr, "build_beat preview: the plan (144 BPM, A minor, sections) without writing anything");
            const auto bb = call ("build_beat", R"({"from_reference":true,"seed":2})");
            te::AudioTrack* t808 = nullptr; for (auto tt : te::getAudioTracks (*session->edit)) if (tt->getName() == "808") t808 = tt;   // findTrack ("808") would read it as a track number
            auto* tDr = session->findTrack ("Drums"); auto* tCh = session->findTrack ("Chords"); auto* tMel = session->findTrack ("Melody");
            int subNotes = 0; if (t808 != nullptr) for (auto c : t808->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c)) subNotes += mc->getSequence().getNumNotes();
            int drumNotes = 0; if (tDr != nullptr) for (auto c : tDr->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c)) drumNotes += mc->getSequence().getNumNotes();
            check (bb.contains ("90 BPM, D minor") && bb.contains ("Written: tracks") && ! bb.contains ("Problems:") && tCh != nullptr && tMel != nullptr && subNotes > 20 && drumNotes > 200,
                   "build_beat from the reference: 90 BPM, D minor, drums (" + juce::String (drumNotes) + " notes), 808 (" + juce::String (subNotes) + "), chords and melody written"
                   + (bb.contains ("Problems:") ? " - " + bb.fromFirstOccurrenceOf ("Problems:", true, false).substring (0, 200) : juce::String()));
            bool presetOk = false; if (tMel != nullptr) for (auto pl : tMel->pluginList) if (Signature::displayName (*pl).contains ("Pollen")) presetOk = true;
            check (presetOk && std::abs (session->edit->tempoSequence.getTempo (0)->getBpm() - 90.0) < 0.01 && (int) session->edit->state.getProperty ("sqmKeyRoot") == 2,
                   "build_beat: Pollen on the melody track, session tempo 90 and key D set");
            const auto cmp2 = call ("analyze_reference", R"({"action":"compare","track":"Ref Beat"})");
            {
                auto mixF = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelchmoth compare mix.wav");
                juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (mixF)); float pk = 0;
                if (rd != nullptr) { juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true); pk = b.getMagnitude (0, b.getNumSamples()); }
                check (bb.contains ("Mix chains:") && bb.contains ("Drums: ") && pk > 0.05f && pk < 0.71f,
                       "build_beat mix chains: " + bb.fromFirstOccurrenceOf ("Mix chains: ", false, false).upToFirstOccurrenceOf (" (starting", false, false).substring (0, 160)
                       + "; the rendered mix peaks at " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS (3 dB or more of headroom)");
            }
            const double mineBpm = cmp2.fromFirstOccurrenceOf ("tempo: ", false, false).fromFirstOccurrenceOf (" vs ", false, false).getDoubleValue();
            const auto mineKey = cmp2.fromFirstOccurrenceOf ("key: ", false, false).fromFirstOccurrenceOf (" vs ", false, false).upToFirstOccurrenceOf ("\n", false, false).upToFirstOccurrenceOf ("  ->", false, false).trim();
            check (std::abs (mineBpm - 90.0) < 1.0, "the built beat, rendered and heard back by the Listener, reads " + juce::String (mineBpm, 1) + " BPM against the reference's 90 (key heard: " + mineKey + ")");
        }
        // ---- Prism Split (Draft): the reference beat separated into stems and a vocal-less instrumental
        {
            const auto sp = call ("separate", R"({"track":"Ref Beat","quality":"draft","wait_seconds":120})");
            int stemTracks = 0; for (auto tt : te::getAudioTracks (*session->edit)) if (tt->getName().startsWith ("Ref Beat - ") && tt->getClips().size() == 1) ++stemTracks;
            const double nullDb = sp.fromFirstOccurrenceOf ("null ", false, false).getDoubleValue();
            check (sp.contains ("done") && stemTracks == 4 && nullDb < -80.0, "separate: 4 stem tracks beside the source, stems sum back to the mix (null " + juce::String (nullDb, 1) + " dB)");
            auto folder = session->getFile().getParentDirectory().getChildFile ("Separated").getChildFile ("Ref Beat");
            auto rmsOf = [] (const juce::File& f) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f)); if (rd == nullptr) return -200.0;
                juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true); return (double) juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -200.0f); };
            const double dB = rmsOf (folder.getChildFile ("Ref Beat - Bass.wav")), dD = rmsOf (folder.getChildFile ("Ref Beat - Drums.wav")), dO = rmsOf (folder.getChildFile ("Ref Beat - Other.wav"));
            check (dB > -40.0 && dD > -45.0 && dO > -60.0, "separate: the 808 lands in Bass (" + juce::String (dB, 1) + " dB RMS), the hits in Drums (" + juce::String (dD, 1) + "), the pad in Other (" + juce::String (dO, 1) + ")");
            // Studio quality: honest when it is not installed; real inference when SQUELCH_STUDIO_PACK points at the pack
            if (! PrismSplit::StudioPack::installed())
            {
                const auto ns = call ("separate", R"({"track":"Ref Beat","quality":"studio"})", false);
                const auto ss = call ("separate", R"({"action":"studio_status"})");
                check (ns.contains ("not installed") && ss.contains ("install_studio"), "separate quality studio without the pack: says how to install it; studio_status explains the download");
               #if JUCE_WINDOWS || JUCE_MAC
                // the installer path on the real platforms: fetch only the runtime (a few MB), verify its checksum, unpack, load it
                const auto ir = call ("separate", R"({"action":"install_studio","runtime_only":true,"wait_seconds":180})");
                juce::DynamicLibrary dl; const bool opened = PrismSplit::StudioPack::libFile().existsAsFile() && dl.open (PrismSplit::StudioPack::libFile().getFullPathName());
                check (ir.contains ("installed") && opened && dl.getFunction ("OrtGetApiBase") != nullptr, "install_studio runtime_only: downloaded, checksum verified, unpacked and loadable (" + PrismSplit::StudioPack::libName() + ")");
                dl.close(); PrismSplit::StudioPack::libFile().deleteFile();
               #endif
            }
            else
            {
                const auto ns = call ("separate", R"({"track":"Ref Beat","quality":"studio","import":false,"wait_seconds":240})");
                const double nd = ns.fromFirstOccurrenceOf ("null ", false, false).getDoubleValue();
                auto sf = session->getFile().getParentDirectory().getChildFile ("Separated").getChildFile ("Ref Beat");
                const double b2 = rmsOf (sf.getChildFile ("Ref Beat - Bass.wav")), d2 = rmsOf (sf.getChildFile ("Ref Beat - Drums.wav"));
                check (ns.contains ("Studio quality") && nd < -80.0 && b2 > -40.0 && d2 > -45.0, "separate quality studio: the neural separator ran in the app (null " + juce::String (nd, 1) + " dB, bass " + juce::String (b2, 1) + ", drums " + juce::String (d2, 1) + " dB RMS)");
            }
            if (! PrismSplit::StudioPack::installed6())
                check (call ("separate", R"({"track":"Ref Beat","stems":6})", false).contains ("6-stem model"), "separate stems 6 without the 6-stem model: says how to install it");
            else
            {
                const auto s6 = call ("separate", R"({"track":"Ref Beat","stems":6,"import":false,"wait_seconds":240})");
                auto sf6 = session->getFile().getParentDirectory().getChildFile ("Separated").getChildFile ("Ref Beat");
                check (s6.contains ("Studio quality") && s6.contains ("Guitar.wav") && s6.contains ("Piano.wav") && sf6.getChildFile ("Ref Beat - Guitar.wav").existsAsFile() && s6.fromFirstOccurrenceOf ("null ", false, false).getDoubleValue() < -80.0,
                       "separate stems 6: Vocals, Drums, Bass, Other, Guitar and Piano written, summing back to the mix (null " + juce::String (s6.fromFirstOccurrenceOf ("null ", false, false).getDoubleValue(), 1) + " dB)");
            }
            const auto st2 = call ("separate", R"({"action":"status","import":false})");
            check (st2.contains ("done") && ! st2.contains ("On the timeline"), "separate status: the finished job again");
        }
        // ---- Sound Library pack 1: every kit in twelve characters
        {
            int rendered = 0, good = 0, distinct = 0; juce::String bad;
            for (int kit : { 2, 12, 15 })
                for (int c = 0; c < DrumKit::numCharacters; ++c)
                    for (int i = 0; i < DrumKit::numSounds; ++i)
                    {
                        const auto d = DrumKit::synthesise (kit, i, 44100.0, c); ++rendered;
                        float pk = 0; double dc = 0, e = 0; bool finite = true;
                        for (auto v : d) { finite = finite && std::isfinite (v); pk = juce::jmax (pk, std::abs (v)); dc += v; e += (double) v * v; }
                        const float tail = d.empty() ? 1.0f : std::abs (d.back());
                        const bool ok = finite && pk > 0.02f && pk <= 1.0f && std::abs (dc / juce::jmax ((size_t) 1, d.size())) < 0.01 && tail < 0.01f;
                        if (ok) ++good; else if (bad.isEmpty()) bad = DrumKit::kitDisplayName (kit, c) + " " + DrumKit::sounds[i].name + " (peak " + juce::String (pk, 3) + ")";
                        if (c > 0) { const auto base = DrumKit::synthesise (kit, i, 44100.0, 0); double diff = 0, be = 0; for (size_t k = 0; k < juce::jmin (d.size(), base.size()); ++k) { diff += (double) (d[k] - base[k]) * (d[k] - base[k]); be += (double) base[k] * base[k]; } if (diff > 0.001 * be || d.size() != base.size()) ++distinct; }
                    }
            check (good == rendered && rendered == 3 * 13 * 12, "Sound Library pack 1: " + juce::String (rendered) + " one-shots rendered from 3 kits x 13 characters, all clean (finite, peak under full scale, no DC, tail at zero)" + (bad.isNotEmpty() ? " - first bad: " + bad : juce::String()));
            int total = 0, same = 0; for (int c = 1; c < DrumKit::numCharacters; ++c) for (int i = 0; i < DrumKit::numSounds; ++i) ++total;
            juce::ignoreUnused (same);
            check (distinct >= 3 * 12 * 12 * 0.5, "each character re-voices the kit: " + juce::String (distinct) + " of " + juce::String (3 * total) + " sounds differ from the kit as designed (a character leaves untouched the sounds it has no say over)");
            const auto ad = call ("add_drum_track", R"({"name":"Char Kit","kit":"Neon Trap - Tight","bars":1})");
            const auto sk = call ("set_drum_kit", R"({"track":"Char Kit","kit":"boom bap","character":"dusty"})");
            int cc = -1; const int kk = DrumKit::kitAndCharacterFor ("R&B Velvet - Roomy", {}, cc);
            check (ad.contains ("Neon Trap - Tight") && sk.contains ("Boom Bap Crates - Dusty") && kk == 15 && cc == 10 && session->sessionsDir.getChildFile ("Kits").getChildFile ("Boom Bap Crates - Dusty").findChildFiles (juce::File::findFiles, false, "*.wav").size() == 12,
                   "add_drum_track / set_drum_kit take a character by name or as 'Kit - Character'; the 12 WAVs are rendered on first use");
        }
        // ---- the Vocal Remover window: drop zone, then two lanes with waveforms once the song is separated
        {
            VocalRemoverContent vr (*session);
            auto shot = [&] (const char* path) { auto img = vr.createComponentSnapshot (vr.getLocalBounds(), true, 1.0f); juce::FileOutputStream os { juce::File (path) }; os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (img, os); return img; };
            const auto empty = shot ("/tmp/main_vocalremover_empty.png");
            vr.start (refF);
            for (int i = 0; i < 1200 && ! vr.isDone() && vr.errorText().isEmpty(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);   // the thumbnails load in the background
            const auto done = shot ("/tmp/main_vocalremover.png");
            auto sf = session->getFile().getParentDirectory().getChildFile ("Separated").getChildFile ("sqm_selftest_reference");
            vr.setStemsMode (true); juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
            const int fourLanes = vr.numLanes(); shot ("/tmp/main_vocalremover_stems.png");
            vr.setStemsMode (false); juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            check (fourLanes == (PrismSplit::StudioPack::installed6() ? 6 : 4) && vr.numLanes() == 2 && vr.keyTempoText().contains ("90 BPM") && vr.keyTempoText().contains ("D minor"),
                   "Vocal Remover window: stems / 2 tracks switch (" + juce::String (fourLanes) + " stem lanes), and the song's tempo and key shown (" + vr.keyTempoText() + ")");
            check (empty.isValid() && vr.isDone() && std::abs (vr.lengthSeconds() - 65.0) < 0.5 && sf.getChildFile ("sqm_selftest_reference - Instrumental.wav").existsAsFile() && sf.getChildFile ("sqm_selftest_reference - Acapella.wav").existsAsFile(),
                   "Vocal Remover window: a dropped song comes back as Music and Vocal lanes (" + juce::String (vr.lengthSeconds(), 1) + " s each)" + (vr.errorText().isNotEmpty() ? " - " + vr.errorText() : juce::String()));
        }
        // ---- make_loop: the loop library writes single parts that fit together
        {
            const auto l1 = call ("make_loop", R"({"part":"drums","style":"boom bap","bars":4,"repeats":2,"seed":5})");
            const auto l2 = call ("make_loop", R"({"part":"chords","style":"boom bap","bars":4,"seed":5,"key":"F minor"})");
            const auto l3 = call ("make_loop", R"({"part":"808","style":"lo-fi","seed":2})");
            te::AudioTrack* dl = nullptr; for (auto tt : te::getAudioTracks (*session->edit)) if (tt->getName() == "Boom Bap drums") dl = tt;
            int dn = 0; double len = 0; if (dl != nullptr) for (auto c : dl->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c)) { dn += mc->getSequence().getNumNotes(); len = mc->getLengthInBeats().inBeats(); }
            check (l1.contains ("Loop written: Boom Bap") && dn > 60 && std::abs (len - 32.0) < 0.01 && l2.contains ("F minor") && l2.contains ("chords") && l3.contains ("808"),
                   "make_loop: a 4-bar boom bap drum loop x2 (" + juce::String (dn) + " notes over " + juce::String (len, 0) + " beats), matching chords in F minor, and a bass loop for a style without an 808");
        }
        // ---- audio_to_midi: the reference beat's drums and 808 as MIDI at the audio's own times
        {
            const auto am = call ("audio_to_midi", R"({"track":"Ref Beat"})");
            te::AudioTrack* dm = nullptr; te::AudioTrack* bm = nullptr;
            for (auto tt : te::getAudioTracks (*session->edit)) { if (tt->getName() == "Ref Beat drums MIDI") dm = tt; if (tt->getName() == "Ref Beat bass MIDI") bm = tt; }
            int k = 0, sn = 0, bn = 0; double firstKick = -1;
            if (dm != nullptr) for (auto c : dm->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c))
                for (auto* n : mc->getSequence().getNotes())
                {
                    if (n->getNoteNumber() == 36) { ++k; const double ts = mc->getPosition().getStart().inSeconds() + n->getStartBeat().inBeats() * 60.0 / session->edit->tempoSequence.getTempo (0)->getBpm(); if (firstKick < 0 || ts < firstKick) firstKick = ts; }
                    if (n->getNoteNumber() == 38) ++sn;
                }
            if (bm != nullptr) for (auto c : bm->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c)) bn += mc->getSequence().getNumNotes();
            const double want = 8 * 4 * 60.0 / 90.0;   // the first kick: bar 9 of the 90 BPM reference
            check (std::abs (k - 48) <= 3 && std::abs (sn - 32) <= 2 && bn >= 12 && bn <= 60 && std::abs (firstKick - want) < 0.03,
                   "audio_to_midi: " + juce::String (k) + " kicks (48 played), " + juce::String (sn) + " snares (32), " + juce::String (bn) + " bass notes (16 played; a kick over a held 808 splits it); the first kick lands at "
                   + juce::String (firstKick, 3) + " s (played at " + juce::String (want, 3) + " s)");
            juce::ignoreUnused (am);
        }
        // ---- hearing in colour and number
        {
            const auto n1 = call ("hearing", R"({"hz":445})");
            check (n1.contains ("A4 +19.6 cents") && n1.contains ("Colour: #") && n1.contains ("Bark") && n1.contains ("ERB number"), "hearing (note): 445 Hz is A4 +19.6 cents, with its colour and critical band - " + n1.upToFirstOccurrenceOf ("\n", false, false));
            const auto n2 = call ("hearing", R"({"action":"harmonics","note":"A2","count":7})");
            check (n2.contains ("3: 330.0 Hz  E4 +2.0 cents  ratio 3:2  (perfect fifth)") && n2.contains ("7: 770.0 Hz  G5 -31.2 cents  ratio 7:4"), "hearing (harmonics): the overtones of A2 with their notes, cents and ratios");
            const auto n3 = call ("hearing", R"({"action":"beats","hz":440,"second_hz":443})");
            check (n3.contains ("beat 3.00 times a second"), "hearing (beats): 440 and 443 Hz beat 3 times a second");
            const auto n4 = call ("hearing", R"({"action":"room","length":5,"width":4,"height":2.5})");
            check (n4.contains ("34.3 Hz") && n4.contains ("42.9 Hz") && n4.contains ("axial (1,0,0)"), "hearing (room): a 5 x 4 x 2.5 m room starts at 34.3 Hz along its length");
            const auto bad = call ("hearing", R"({"note":"hello"})", false);
            check (bad.contains ("Give hz"), "hearing refuses something that is neither a note nor a frequency");
            HearingContent hc; hc.setSize (980, 640);
            hc.setFrequency (445.0);
            const auto shot = hc.createComponentSnapshot (hc.getLocalBounds(), true, 1.0f);
            { const juce::File png ("/tmp/main_hearing.png"); png.deleteFile(); juce::FileOutputStream os (png); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shot, os); }
            const auto a4 = SqHear::noteColour (69, -6.0); int found = 0;
            for (int y = 0; y < shot.getHeight(); y += 2) for (int x = 0; x < shot.getWidth(); x += 2) { const auto px = shot.getPixelAt (x, y); if (std::abs (px.getRed() - a4.r) <= 2 && std::abs (px.getGreen() - a4.g) <= 2 && std::abs (px.getBlue() - a4.b) <= 2) ++found; }
            check (hc.readout().startsWith ("A4 +19.6 cents, 445.00 Hz, colour #") && found > 400, "Hearing window: shows A4 +19.6 cents and paints the note in its colour (" + juce::String (found) + " sampled pixels of " + a4.hex() + ")");
            hc.setVision (SqHear::Vision::deutan);
            const auto shot2 = hc.createComponentSnapshot (hc.getLocalBounds(), true, 1.0f);
            const auto seen = SqHear::seenAs (a4, SqHear::Vision::deutan); int found2 = 0;
            for (int y = 0; y < shot2.getHeight(); y += 2) for (int x = 0; x < shot2.getWidth(); x += 2) { const auto px = shot2.getPixelAt (x, y); if (std::abs (px.getRed() - seen.r) <= 2 && std::abs (px.getGreen() - seen.g) <= 2 && std::abs (px.getBlue() - seen.b) <= 2) ++found2; }
            check (found2 > 400, "Hearing window: the green-blind view repaints the same note as that viewer sees it (" + seen.hex() + ")");
        }
        // ---- unmask: two tracks in the same critical band, found and carved
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sq-unmask-selftest"); dir.deleteRecursively(); dir.createDirectory();
            auto writeTones = [&] (const juce::String& name, std::initializer_list<std::pair<double, double>> parts)
            {
                const double fs = 48000.0; const int n = 48000 * 4; juce::AudioBuffer<float> b (1, n); b.clear();
                for (auto& p : parts) for (int i = 0; i < n; ++i) b.getWritePointer (0)[i] += (float) (p.second * std::sin (2.0 * juce::MathConstants<double>::pi * p.first * i / fs));
                const auto f = dir.getChildFile (name + ".wav"); juce::WavAudioFormat wav;
                if (auto os = std::make_unique<juce::FileOutputStream> (f); os->openedOk())
                    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), fs, 1, 24, {}, 0))) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
                return f;
            };
            const auto fa = writeTones ("Unmask A", { { 230.0, 0.4 }, { 3000.0, 0.1 } }), fb = writeTones ("Unmask B", { { 260.0, 0.2 }, { 800.0, 0.1 } });
            auto addWith = [&] (const juce::String& name, const juce::File& f)
            {
                const auto r = call ("add_track", "{\"kind\":\"audio\",\"name\":\"" + name + "\"}"); int idx = 0;
                for (int i = 0; i < r.length(); ++i) if (juce::CharacterFunctions::isDigit (r[i])) { idx = r.substring (i).getIntValue(); break; }
                call ("import_audio", "{\"track\":" + juce::String (idx) + ",\"start_seconds\":0,\"path\":" + juce::JSON::toString (juce::var (f.getFullPathName())) + "}");
                return idx;
            };
            const int ia = addWith ("Unmask A", fa), ib = addWith ("Unmask B", fb);
            const auto args = "\"track_a\":" + juce::String (ia) + ",\"track_b\":" + juce::String (ib) + ",\"start_seconds\":0,\"end_seconds\":4";
            const auto u1 = call ("unmask", "{" + args + "}");
            check (u1.contains ("overlap from 200 to 300 Hz") && u1.contains ("keep Unmask A") && u1.contains ("cut Unmask B") && u1.contains ("at 245 Hz") && u1.contains ("Nothing was changed"),
                   "unmask finds the shared 200-300 Hz band of two test tracks and proposes the cut - " + u1.fromFirstOccurrenceOf ("Starting point: ", false, false).upToFirstOccurrenceOf ("\n", false, false));
            const auto u2 = call ("unmask", "{" + args + ",\"apply\":true}");
            const double was = u2.fromFirstOccurrenceOf ("(was ", false, false).getDoubleValue(), now = u2.fromFirstOccurrenceOf ("now measures ", false, false).getDoubleValue();
            bool hasEq = false; if (auto tb2 = session->findTrack (juce::var (ib))) for (auto p : tb2->pluginList) if (dynamic_cast<te::EqualiserPlugin*> (p) != nullptr) hasEq = true;
            check (hasEq && u2.contains ("Done: a Prism EQ") && now < was - 1.5 && now > was - 8.0, "unmask apply: the EQ is on the quieter track and that band came down from " + juce::String (was, 1) + " to " + juce::String (now, 1) + " dBFS");
            const auto u3 = call ("unmask", "{\"track_a\":" + juce::String (ia) + ",\"track_b\":" + juce::String (ia) + "}", false);
            check (u3.contains ("same track"), "unmask refuses one track against itself");
            const auto c1 = call ("hearing", R"({"action":"colour_notes","on":true})"), c2 = call ("hearing", R"({"action":"colour_notes","on":false})");
            check (c1.contains ("coloured by pitch") && c2.contains ("coloured by velocity"), "piano-roll note colours switch between pitch and velocity, and back to the standard look");
            // the Hearing window's Unmask panel, wired the way the app wires it
            {
                HearingContent hv; attachHearing (hv, tools, *session); hv.setSize (980, 836);
                hv.chooseTracks (ia, ib); hv.measure (false);
                const auto& v = hv.unmaskView();
                check (v.ok && v.overlap.found && v.overlap.firstBand == 2 && v.nameA == "Unmask A" && v.nameB == "Unmask B" && v.message.contains ("overlap from 200 to 300 Hz"),
                       "Hearing window > Unmask: picks the two tracks, measures, and holds the same overlap the tool reports");
                const auto shot = hv.createComponentSnapshot (hv.getLocalBounds(), true, 1.0f);
                { const juce::File png ("/tmp/main_hearing_unmask.png"); png.deleteFile(); juce::FileOutputStream os (png); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shot, os); }
                int cyan = 0, pink = 0; const auto ca = Console::Look::cyan(), cb = Console::Look::pink();
                for (int y = hv.getHeight() - 190; y < hv.getHeight() - 50; ++y) for (int x = 20; x < hv.getWidth() - 20; ++x)
                { const auto px = shot.getPixelAt (x, y); if (std::abs (px.getRed() - ca.getRed()) < 40 && std::abs (px.getGreen() - ca.getGreen()) < 40 && std::abs (px.getBlue() - ca.getBlue()) < 40) ++cyan;
                  if (std::abs (px.getRed() - cb.getRed()) < 40 && std::abs (px.getGreen() - cb.getGreen()) < 40 && std::abs (px.getBlue() - cb.getBlue()) < 40) ++pink; }
                check (cyan > 300 && pink > 300, "Hearing window > Unmask: both tracks are drawn across the critical bands (" + juce::String (cyan) + " and " + juce::String (pink) + " pixels)");
                hv.chooseTracks (ia, ia); hv.measure (false);
                check (hv.unmaskView().message == "Choose two different tracks.", "Hearing window > Unmask: the same track twice is refused in plain words");
            }
            // the two test files stay on disk: the session's tracks still play them
        }
        // ---- the advisor: measured advice on the mix and on one take
        {
            const auto adv = call ("session_advice", R"({"start_seconds":0,"end_seconds":12})");
            check (adv.startsWith ("Mix, 0.0 to 12.0 s") && adv.contains ("tracks measured") && adv.contains ("Bands: sub") && adv.contains ("peak ") && ! adv.contains ("0 tracks measured"),
                   "session_advice (mixing): the mix and its tracks measured - " + adv.upToFirstOccurrenceOf ("\n", false, false).substring (0, 150));
            const auto rec = call ("session_advice", R"({"stage":"recording","track":"Ref Beat"})");
            check (rec.startsWith ("Take on Ref Beat") && rec.contains ("quietest moments"), "session_advice (recording): the take on a track measured - " + rec.upToFirstOccurrenceOf ("\n", false, false).substring (0, 150));
        }
        // ---- Frame, stage 0: video in and out (needs FFmpeg on the machine; without it the tools must say so)
        {
            if (! Frame::ffmpeg().existsAsFile() && juce::SystemStats::getEnvironmentVariable ("SQUELCH_TEST_VIDEO_INSTALL", {}) == "1")
            {
                // asked for by the test machine: run the one-time download for real, then every video check below runs on it
                call ("video_export", R"({"action":"install","wait_seconds":5})");
                Frame::installState().done.wait (900000);
                check (Frame::installState().error.isEmpty() && Frame::ffmpeg().existsAsFile(),
                       "video tools download: FFmpeg fetched, its checksum matched and it runs on this machine" + (Frame::installState().error.isNotEmpty() ? " - " + Frame::installState().error : juce::String()));
            }
            const auto ff = Frame::ffmpeg();
            const auto in1 = Frame::parseInfo ("  Duration: 01:02:03.50, start: 0.000000, bitrate: 512 kb/s\n  Stream #0:0[0x1](und): Video: h264 (High) (avc1 / 0x31637661), yuv420p(progressive), 1920x1080 [SAR 1:1 DAR 16:9], 4500 kb/s, 29.97 fps, 30 tbr (default)\n  Stream #0:1[0x2](und): Audio: aac (LC) (mp4a / 0x6134706D), 48000 Hz, stereo, fltp, 192 kb/s\n");
            check (in1.ok && in1.width == 1920 && in1.height == 1080 && std::abs (in1.fps - 29.97) < 0.001 && std::abs (in1.seconds - 3723.5) < 0.01 && in1.videoCodec == "h264" && in1.audioCodec == "aac",
                   "Frame reads a video's facts: " + in1.text());
            if (! ff.existsAsFile())
            {
                // the download itself is not run by the self-test: it fetches and starts a third-party program
                const auto r = call ("video_export", R"({"picture":"waveform","start_seconds":0,"end_seconds":2})", false);
                check (r.contains ("action install"), "video_export without FFmpeg on this machine says how to get it (action install)");
                const auto st = call ("video_export", R"({"action":"install_status"})");
                check (st.contains ("not installed") && juce::String (Frame::packItem().sha256).length() == 64, "video tools: install_status reports not installed; the download is pinned to a SHA-256");
            }
            else
            {
                const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sq-frame-selftest"); dir.deleteRecursively(); dir.createDirectory();
                const auto clip = dir.getChildFile ("clip.mp4"); juce::String out;
                Frame::run ({ ff.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc=s=320x240:r=25:d=3", "-f", "lavfi", "-i", "sine=f=440:d=3",
                              "-c:v", Frame::videoEncoder(), "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", clip.getFullPathName() }, out);
                check (clip.getSize() > 2000, "Frame: FFmpeg found (" + ff.getFullPathName() + ", encoder " + Frame::videoEncoder() + ") and a test clip made");
               #if JUCE_MAC
                {
                    const auto nat = dir.getChildFile ("native.wav");
                    const bool ok = Frame::nativeAudio (clip, nat);
                    check (ok && nat.getSize() > 100000, "macOS reads the sound of an MP4 itself, without FFmpeg (" + juce::String (nat.getSize() / 1024) + " KB)");
                }
               #endif
                const int tracksBefore = te::getAudioTracks (*session->edit).size();
                const auto vi = call ("video_import", "{\"path\":" + juce::JSON::toString (juce::var (clip.getFullPathName())) + "}");
                check (vi.contains ("320 x 240") && vi.contains ("25 fps") && te::getAudioTracks (*session->edit).size() == tracksBefore + 1, "video_import: facts read and the sound on a new track - " + vi.upToFirstOccurrenceOf ("\n", false, false));
                const auto asJson = [] (const juce::File& f) { return juce::JSON::toString (juce::var (f.getFullPathName())); };
                const auto o1 = dir.getChildFile ("source.mp4");
                const auto e1 = call ("video_export", "{\"picture\":\"source\",\"start_seconds\":0,\"end_seconds\":3,\"wait_seconds\":200,\"path\":" + asJson (o1) + "}");
                const auto p1 = Frame::probe (o1);
                check (e1.contains ("without re-encoding") && p1.ok && p1.hasVideo && p1.hasAudio && p1.width == 320 && std::abs (p1.seconds - 3.0) < 0.35, "video_export picture source: the new mix under the original picture - " + p1.text());
                const auto o2 = dir.getChildFile ("wave.mp4");
                const auto e2 = call ("video_export", "{\"picture\":\"waveform\",\"width\":640,\"height\":360,\"start_seconds\":0,\"end_seconds\":3,\"wait_seconds\":200,\"path\":" + asJson (o2) + "}");
                const auto p2 = Frame::probe (o2);
                check (e2.contains ("exported") && p2.ok && p2.width == 640 && p2.height == 360 && p2.hasAudio && std::abs (p2.seconds - 3.0) < 0.35, "video_export picture waveform 640 x 360 - " + p2.text());
                const auto sv = call ("separate", "{\"quality\":\"draft\",\"import\":false,\"wait_seconds\":120,\"file\":" + asJson (clip) + ",\"folder\":" + asJson (dir) + "}");
                check (sv.contains ("done"), "separate takes a video file: its sound is separated");
                // ---- the picture: exact frames, and the Video window following a clock
                {
                    // a clip whose brightness rises frame by frame, so the frame on screen can be read back from the picture
                    // (the frames are written here, grey level 4 + 2.5 per frame, so no FFmpeg filter decides what they contain)
                    const auto ramp = dir.getChildFile ("ramp.mp4"), rawFrames = dir.getChildFile ("ramp.rgb"); juce::String o2;
                    {
                        juce::FileOutputStream os (rawFrames); juce::HeapBlock<juce::uint8> fr (64 * 64 * 3);
                        for (int k = 0; k < 100; ++k) { std::memset (fr, juce::roundToInt (4.0 + 2.5 * k), 64 * 64 * 3); os.write (fr, 64 * 64 * 3); }
                    }
                    Frame::run ({ ff.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "64x64", "-r", "25", "-i", rawFrames.getFullPathName(),
                                  "-f", "lavfi", "-i", "sine=f=330:d=4", "-c:v", Frame::videoEncoder(), "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", ramp.getFullPathName() }, o2);
                    const auto in = Frame::probe (ramp);
                    auto frameOf = [] (const juce::Image& img) { return img.isValid() ? ((double) img.getPixelAt (img.getWidth() / 2, img.getHeight() / 2).getGreen() - 4.0) / 2.5 : -100.0; };
                    juce::String gerr; bool exact = in.ok && std::abs (in.fps - 25.0) < 0.01; juce::String seen;
                    for (double t : { 0.02, 1.0, 2.52, 3.9 })
                    {
                        const auto img = Frame::grabFrame (ramp, in, t, 64, gerr); const double got = frameOf (img), want = (double) Frame::frameIndexAt (t, 25.0);
                        seen << juce::String (t, 2) << " s -> frame " << juce::String (got, 1) << " (" << juce::String ((int) want) << ")  ";
                        if (std::abs (got - want) > 1.2) exact = false;
                    }
                    check (exact, "Frame: the frame fetched for a time is the one on screen then - " + seen.trim());
                    call ("new_session", R"({"name":"Selftest Video"})");
                    const auto vi2 = call ("video_import", "{\"path\":" + asJson (ramp) + ",\"start_seconds\":2}");
                    const auto png = dir.getChildFile ("frame.png");
                    const auto vf = call ("video_frame", "{\"seconds\":3.0,\"path\":" + asJson (png) + "}");
                    const auto saved = juce::ImageFileFormat::loadFrom (png);
                    check (vi2.contains ("64 x 64") && vf.contains ("frame 25 of ramp.mp4") && vf.contains ("00:00:01:00 in the video") && saved.isValid() && std::abs (frameOf (saved) - 25.0) <= 1.2,
                           "video_frame: 3.0 s of the session is frame 25 of a video placed at 2 s, and the saved picture is that frame - " + vf.upToFirstOccurrenceOf ("\n", false, false));
                    const auto before = call ("video_frame", R"({"seconds":1.0})"), after = call ("video_frame", R"({"seconds":7.5})");
                    check (before.contains ("has not started") && after.contains ("has ended"), "video_frame says so before the picture starts and after it ends");

                    VideoContent vc; double clock = 2.0; bool playing = false;
                    vc.readState = [&] { VideoContent::State s; s.video = ramp; s.videoStartsAt = 2.0; s.playhead = clock; s.playing = playing; return s; };
                    vc.setSize (880, 560);
                    auto pump = [&] (int ms) { const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms; while (juce::Time::getMillisecondCounter() < end) { vc.update(); juce::Thread::sleep (15); } };
                    clock = 4.0; pump (100); for (int i = 0; i < 200 && vc.frameShown() != 50; ++i) pump (30);
                    check (vc.frameShown() == 50 && std::abs (frameOf (vc.imageShown()) - 50.0) <= 1.2, "Video window, stopped: the playhead at 4.0 s shows frame 50 exactly (the picture reads " + juce::String (frameOf (vc.imageShown()), 1) + ")");
                    { const auto shot = vc.createComponentSnapshot (vc.getLocalBounds(), true, 1.0f); const juce::File sp ("/tmp/main_video.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shot, os); }
                    // play from 2.4 s for a second and a half of real time: the picture must keep up with the clock
                    playing = true; const double t0 = 2.4; const auto started = juce::Time::getMillisecondCounterHiRes();
                    int samples = 0, close = 0, distinct = 0; juce::int64 last = -1; bool backwards = false; double worst = 0;
                    while (juce::Time::getMillisecondCounterHiRes() - started < 1500.0)
                    {
                        clock = t0 + (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0; vc.update();
                        const auto shown = vc.frameShown(), want = Frame::frameIndexAt (clock - 2.0, 25.0);
                        if (juce::Time::getMillisecondCounterHiRes() - started > 600.0 && shown >= 0)
                        {
                            ++samples; const double off = (double) std::abs (shown - want); worst = std::max (worst, off); if (off <= 2.0) ++close;
                            if (shown != last) { if (shown < last) backwards = true; ++distinct; last = shown; }
                        }
                        juce::Thread::sleep (12);
                    }
                    playing = false; vc.update();
                    check (samples > 20 && distinct >= 12 && ! backwards && close >= samples * 8 / 10,
                           "Video window, playing: the picture follows the clock - " + juce::String (distinct) + " frames in order over 0.9 s, " + juce::String (close) + " of " + juce::String (samples) + " looks within two frames (worst " + juce::String (worst, 0) + ")");
                    clock = 9.0; pump (150);
                    check (vc.frameShown() == -1 && ! vc.imageShown().isValid(), "Video window: past the end of the video there is no picture, and it says so");
                    // ---- 1.29: the strip of stills, click to go there, step by frame, and the contact sheet
                    {
                        clock = 3.0; pump (60);
                        auto& st = vc.getStrip(); const double secs = vc.getPlayer().facts().seconds;
                        for (int i = 0; i < 600 && ! (st.size() > 0 && st.finished()); ++i) pump (50);
                        const int n = st.size(); int right = 0; double worstOff = 0;
                        for (int k = 0; k < n; ++k)
                        {
                            const double off = std::abs (frameOf (st.thumb (k)) - (double) Frame::frameIndexAt (SqStrip::slotTime (k, n, secs), 25.0));
                            worstOff = std::max (worstOff, off); if (off <= 1.2) ++right;
                        }
                        check (n >= 8 && st.done() == n && right == n, "Video strip: " + juce::String (n) + " stills, each the frame at the middle of its slot (" + juce::String (right) + " right, worst " + juce::String (worstOff, 1) + " frames off)");
                        pump (60);
                        { const auto shot = vc.createComponentSnapshot (vc.getLocalBounds(), true, 1.0f); const juce::File sp ("/tmp/main_video_strip.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shot, os); }
                        vc.seek = [&] (double t) { clock = t; };
                        const auto sb = vc.stripBounds();
                        const double went = vc.seekToStripX (sb.getX() + sb.getWidth() / 4);
                        const auto wantIdx = Frame::frameIndexAt (secs / 4.0, 25.0);
                        pump (100); for (int i = 0; i < 200 && vc.frameShown() != wantIdx; ++i) pump (30);
                        check (std::abs (went - (2.0 + secs / 4.0)) < 0.01 && std::abs (clock - went) < 1.0e-9 && vc.frameShown() == wantIdx,
                               "Video strip: a click a quarter of the way along goes to " + juce::String (went, 2) + " s of the session and the picture is frame " + juce::String (vc.frameShown()));
                        clock = 3.0; vc.update();
                        const bool k1 = vc.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)); const double afterRight = clock;
                        vc.update(); vc.keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0)); const double afterShiftLeft = clock;
                        vc.update(); vc.keyPressed (juce::KeyPress (juce::KeyPress::homeKey)); const double afterHome = clock;
                        vc.update(); vc.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)); const double atStart = clock;
                        vc.update(); vc.keyPressed (juce::KeyPress (juce::KeyPress::endKey)); const double afterEnd = clock;
                        check (k1 && std::abs (afterRight - (2.0 + 26.0 / 25.0)) < 1.0e-9 && std::abs (afterShiftLeft - (2.0 + 1.0 / 25.0)) < 1.0e-9 && std::abs (afterHome - 2.0) < 1.0e-9 && std::abs (atStart - 2.0) < 1.0e-9
                               && std::abs (afterEnd - (2.0 + (double) (SqStrip::frameCount (secs, 25.0) - 1) / 25.0)) < 1.0e-9,
                               "Video window keys: right is one frame (frame 26), Shift+left one second back, Home the start, left at the start stays, End the last frame (" + juce::String (afterEnd, 2) + " s)");
                        const auto stp = call ("video_frame", R"({"seconds":3.0,"step":2})");
                        const double ph = session->edit != nullptr ? session->edit->getTransport().getPosition().inSeconds() : -1.0;
                        check (stp.contains ("Playhead moved to 3.080") && stp.contains ("frame 27 of ramp.mp4") && std::abs (ph - 3.08) < 1.0e-6, "video_frame step 2 from 3.0 s: the playhead is at 3.08 s on frame 27");
                        const auto sheetPng = dir.getChildFile ("sheet.png");
                        const auto shr = call ("video_frame", "{\"sheet\":6,\"columns\":3,\"width\":64,\"path\":" + asJson (sheetPng) + "}");
                        const auto sheetImg = juce::ImageFileFormat::loadFrom (sheetPng); const auto lay = SqStrip::sheet (6, 3, 64, 64, 64);
                        int cellsRight = 0;
                        if (sheetImg.isValid() && sheetImg.getWidth() == lay.width && sheetImg.getHeight() == lay.height)
                            for (int k = 0; k < 6; ++k)
                            {
                                const double got = ((double) sheetImg.getPixelAt (lay.cellX (k) + lay.cellW / 2, lay.cellY (k) + lay.imageH / 2).getGreen() - 4.0) / 2.5;
                                if (std::abs (got - (double) Frame::frameIndexAt (SqStrip::slotTime (k, 6, secs), 25.0)) <= 1.2) ++cellsRight;
                            }
                        check (shr.contains ("6 stills") && shr.contains ("1. 00:00:00:0") && cellsRight == 6, "video_frame sheet: 6 stills in 3 columns, " + juce::String (sheetImg.getWidth()) + " x " + juce::String (sheetImg.getHeight())
                               + ", every cell the frame for its time (" + juce::String (cellsRight) + " of 6), times listed");
                        const auto noPath = call ("video_frame", R"({"sheet":6})", false);
                        check (noPath.contains ("needs path"), "video_frame sheet without a path says what is missing");
                    }
                    // ---- 1.30: the video on the timeline - moving it (tool, drag, keys) and the lane of stills
                    if (session->edit != nullptr)
                    {
                        auto& ed = *session->edit;
                        auto at = [&] { return (double) ed.state.getProperty ("frameVideoAt", -1.0); };
                        auto snd = [&] { auto* c = VideoMove::soundClip (ed); return c != nullptr ? c->getPosition().getStart().inSeconds() : -1.0; };
                        const double at0 = at(), snd0 = snd();
                        const auto mv = call ("video_move", R"({"start_seconds":5})");
                        const auto vf5 = call ("video_frame", R"({"seconds":6.0})");
                        check (std::abs (at0 - 2.0) < 1.0e-9 && std::abs (snd0 - 2.0) < 1.0e-6 && std::abs (at() - 5.0) < 1.0e-9 && std::abs (snd() - 5.0) < 1.0e-6 && mv.contains ("now starts at 5.000 s") && vf5.contains ("frame 25 of ramp.mp4"),
                               "video_move to 5 s: the picture and its sound clip both start at 5 s, and 6.0 s of the session is now frame 25");
                        ed.getUndoManager().undo(); const double atU = at(), sndU = snd();
                        ed.getUndoManager().redo();
                        check (std::abs (atU - 2.0) < 1.0e-9 && std::abs (sndU - 2.0) < 1.0e-6 && std::abs (at() - 5.0) < 1.0e-9 && std::abs (snd() - 5.0) < 1.0e-6, "video_move is one undo step: undo puts picture and sound back at 2 s (" + juce::String (atU, 2) + " / " + juce::String (sndU, 2) + "), redo returns them to 5 s");
                        call ("video_move", R"({"by_frames":-25})"); const double atF = at(), sndF = snd();
                        const auto slip = call ("video_move", R"({"by_seconds":0.5,"sound":false})");
                        check (std::abs (atF - 4.0) < 1.0e-9 && std::abs (sndF - 4.0) < 1.0e-6 && std::abs (at() - 4.5) < 1.0e-9 && std::abs (snd() - 4.0) < 1.0e-6 && slip.contains ("left where it is"),
                               "video_move by -25 frames is one second back (4 s); sound false slips only the picture (4.5 s, sound still at 4 s)");
                        call ("video_move", R"({"by_seconds":-0.5,"sound":false})");
                        call ("video_move", R"({"start_seconds":-3})"); const double atNeg = at();
                        const auto noArg = call ("video_move", "{}", false);
                        check (std::abs (atNeg) < 1.0e-9 && noArg.contains ("Say where"), "video_move never goes before the start of the session, and says what it needs when given nothing");
                        call ("video_move", R"({"start_seconds":1})");

                        TimelineState ts; ts.viewStart = 0.0; ts.secondsPerPixel = 0.01;
                        VideoLane lane (*session, ts); lane.setBounds (0, 0, headerWidthNow() + 900, VideoLane::laneH); lane.setVisible (true);
                        for (int i = 0; i < 800 && ! (lane.getStrip().size() > 0 && lane.getStrip().finished()); ++i) { lane.refresh(); juce::Thread::sleep (40); }
                        lane.refresh();
                        const double secs = lane.facts().seconds; const int n = lane.getStrip().size(); const auto bx = lane.blockX();
                        const auto shot = lane.createComponentSnapshot (lane.getLocalBounds(), true, 1.0f);
                        { const juce::File sp ("/tmp/main_video_lane.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shot, os); }
                        const double tileW = (double) VideoLane::laneH - 7.0;
                        const auto tl = SqStrip::tiles ((double) bx.getStart(), (double) bx.getEnd(), (double) headerWidthNow(), (double) lane.getWidth(), tileW, n);
                        int tilesRight = 0;
                        for (auto& t : tl)
                        {
                            const double got = ((double) shot.getPixelAt (juce::roundToInt (t.x + t.w * 0.5), VideoLane::laneH / 2).getGreen() - 4.0) / 2.5;
                            if (std::abs (got - (double) Frame::frameIndexAt (SqStrip::slotTime (t.thumb, n, secs), 25.0)) <= 1.5) ++tilesRight;
                        }
                        const auto outside = shot.getPixelAt (juce::roundToInt (bx.getStart()) - 20, VideoLane::laneH / 2);
                        check (lane.wanted() && n >= 50 && std::abs (bx.getStart() - ((float) headerWidthNow() + 100.0f)) < 0.6f && std::abs (bx.getLength() - (float) (secs * 100.0)) < 6.0f && tl.size() >= 8 && tilesRight == (int) tl.size()
                               && outside == Theme::bg(),
                               "Video lane: the block sits at 1 s and is as long as the video; " + juce::String (tilesRight) + " of " + juce::String ((int) tl.size()) + " stills along it are the frames for their place; nothing is drawn before it");
                        lane.dragBy (150, true); const auto dropped = lane.dropDrag(); const double atDrag = at();
                        const bool kr = lane.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)); const double atKey = at();
                        lane.keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0)); const double atShift = at();
                        lane.dragBy (-100000, true); lane.dropDrag(); const double atFarLeft = at();
                        check (std::abs (atDrag - 2.5) < 1.0e-9 && dropped.contains ("now starts at 2.500 s") && kr && std::abs (atKey - 2.54) < 1.0e-9 && std::abs (atShift - 1.54) < 1.0e-9 && std::abs (atFarLeft) < 1.0e-9,
                               "Video lane: dragging the block 150 px at 100 px a second moves the video 1.5 s; right arrow one frame, Shift+left one second; a drag far left stops at 0");
                        Prefs::set ("ed_videoLane", 0); const bool hidden = ! lane.wanted(); Prefs::set ("ed_videoLane", 1);
                        check (hidden && lane.wanted(), "View > Video Lane hides the lane and brings it back");
                        // the lane inside the real timeline: under the rulers, above the tracks
                        {
                            call ("video_move", R"({"start_seconds":1})");
                            TimelineView tlv (*session); tlv.setSize (1400, 620);
                            auto& vl = tlv.getVideoLane();
                            for (int i = 0; i < 800 && ! (vl.getStrip().size() > 0 && vl.getStrip().finished()); ++i) { vl.refresh(); juce::Thread::sleep (40); }
                            vl.refresh();
                            const auto shotT = tlv.createComponentSnapshot (tlv.getLocalBounds(), true, 1.0f);
                            { const juce::File sp ("/tmp/main_timeline_video.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shotT, os); }
                            const bool shown = vl.isVisible() && vl.getHeight() == VideoLane::laneH && vl.getWidth() == 1400;
                            Prefs::set ("ed_videoLane", 0); tlv.resized(); const bool gone = ! vl.isVisible(); Prefs::set ("ed_videoLane", 1); tlv.resized();
                            check (shown && gone && vl.isVisible(), "Timeline: the video lane takes its place under the rulers when the session has a video, and leaves when hidden (lane at y " + juce::String (vl.getY()) + ")");
                        }
                    }
                    // ---- 1.31: proxies - made in parts, the same frame numbers, found again, removed
                    {
                        auto& px = Frame::Proxies::get(); px.setPartSecondsForTests (1.0); px.setPaused (false);
                        px.open (ramp, in, false); px.remove (ramp);
                        const auto none = call ("video_proxy", "{}");
                        const auto made = call ("video_proxy", R"({"action":"make","wait_seconds":180})");
                        auto st = px.status (ramp);
                        const int parts = SqProxy::partCount (SqStrip::frameCount (in.seconds, in.fps), 25);
                        check (none.contains ("No proxy") && parts >= 4 && st.complete() && st.parts == parts && st.bytes > 0 && made.contains (juce::String (parts) + " of " + juce::String (parts) + " parts"),
                               "video_proxy make: a 4-second video in 1-second parts gives " + juce::String (st.parts) + " parts, all made - " + made);
                        int right = 0, asked = 0; juce::String seenP;
                        for (juce::int64 f : { 0, 1, 24, 25, 26, 49, 50, 63, 98, 99 })
                        {
                            ++asked; const auto img = px.grab (ramp, f, 64, 64); const double got = frameOf (img);
                            if (px.ready (ramp, f) && std::abs (got - (double) f) <= 1.2) ++right;
                            seenP << f << "->" << juce::String (got, 1) << " ";
                        }
                        check (right == asked, "Proxy frames are the video's frames, across part joins too (frame -> read back): " + seenP.trim());
                        juce::String ge; const auto viaAny = Frame::grabFrame (ramp, in, 2.52, 64, ge), viaVideo = Frame::grabFrame (ramp, in, 2.52, 64, ge, false);
                        check (std::abs (frameOf (viaAny) - 63.0) <= 1.2 && std::abs (frameOf (viaVideo) - 63.0) <= 1.2 && ! px.grab (ramp, 63, 128, 128).isValid() && ! px.grab (ramp, 5000, 64, 64).isValid(),
                               "a still at 2.52 s is frame 63 with the proxy and from the video itself; the proxy is not used for a size bigger than it or a frame past the end");
                        // found again in a later session, and a missing part is made again without redoing the others
                        const auto part2 = px.partPath (ramp, 2), part0 = px.partPath (ramp, 0); const auto stamp0 = part0.getLastModificationTime();
                        px.forget (ramp); part2.deleteFile();
                        const auto st2 = px.open (ramp, in, false);
                        const bool gap = st2.made == parts - 1 && ! px.ready (ramp, 63) && px.ready (ramp, 10) && ! px.grab (ramp, 63, 64, 64).isValid();
                        const auto fallback = Frame::grabFrame (ramp, in, 2.52, 64, ge);
                        const auto again = call ("video_proxy", R"({"action":"make","wait_seconds":180})");
                        check (gap && std::abs (frameOf (fallback) - 63.0) <= 1.2 && px.status (ramp).complete() && part2.existsAsFile() && part0.getLastModificationTime() == stamp0,
                               "Proxy parts are found again; with one part missing, stills there come from the video until that part alone is remade (" + juce::String (st2.made) + " of " + juce::String (parts) + " found)");
                        const auto rm = call ("video_proxy", R"({"action":"remove"})");
                        const bool goneNow = ! px.status (ramp).known && ! part0.existsAsFile() && ! part0.getParentDirectory().isDirectory();
                        // the P key in the Video window asks for the same thing
                        clock = 3.0; vc.update();
                        const bool pk = vc.keyPressed (juce::KeyPress ('p', juce::ModifierKeys(), 'p'));
                        for (int i = 0; i < 3000 && ! px.status (ramp).complete(); ++i) juce::Thread::sleep (20);
                        const bool byKey = px.status (ramp).complete();
                        vc.removeProxy();
                        check (rm.contains ("removed") && goneNow && pk && byKey && ! px.status (ramp).known, "video_proxy remove deletes the files; the P key in the Video window makes the proxy, and its Remove takes it away again");
                        const auto offR = call ("video_proxy", R"({"action":"auto","on":false})"); const bool wasOff = ! VideoProxy::autoOn();
                        const auto onR = call ("video_proxy", R"({"action":"auto","on":true})");
                        check (wasOff && VideoProxy::autoOn() && offR.contains ("only made when asked") && onR.contains ("without asking") && ! SqProxy::worthIt (in.width, in.seconds), "video_proxy auto switches unasked proxies off and on; a small short video never gets one unasked");
                        px.setPartSecondsForTests (300.0);
                    }
                    // ---- 1.32: cutting the picture by cutting its sound
                    if (session->edit != nullptr)
                    {
                        auto& ed = *session->edit;
                        call ("video_move", R"({"start_seconds":2})");
                        auto pieces = [&] { return VideoCuts::read (ed, in.seconds); };
                        const auto first = VideoCuts::clips (ed);
                        const juce::String trackName = ! first.empty() && first[0]->getTrack() != nullptr ? first[0]->getTrack()->getName() : juce::String ("?");
                        const auto trk = "\"track\":" + juce::JSON::toString (juce::var (trackName));
                        const bool oneBefore = pieces().pieces.size() == 1 && pieces().followsSound && std::abs (pieces().pieces[0].at - 2.0) < 1.0e-6;
                        call ("split_clip", "{" + trk + ",\"at_seconds\":3.0}"); call ("split_clip", "{" + trk + ",\"at_seconds\":4.0}");
                        const auto three = pieces(); const auto cutsText = call ("video_cuts", "{}");
                        const auto mid = call ("video_frame", R"({"seconds":3.5})");
                        check (oneBefore && three.pieces.size() == 3 && std::abs (three.pieces[1].at - 3.0) < 1.0e-6 && std::abs (three.pieces[1].srcIn - 1.0) < 1.0e-6 && std::abs (three.pieces[2].srcIn - 2.0) < 1.0e-6
                               && cutsText.contains ("3 pieces") && mid.contains ("frame 37 of ramp.mp4"),
                               "Cutting the video's sound at 3 s and 4 s makes three pieces of picture (the second starts on the video's 1 s); nothing has moved, so 3.5 s still shows frame 37");
                        auto cl = VideoCuts::clips (ed);
                        if (cl.size() == 3)
                        {
                            call ("delete_clip", "{" + trk + ",\"clip\":2}");                              // clips are numbered in time order on the track
                            call ("edit_clip", "{" + trk + ",\"clip\":2,\"start_seconds\":8.0}");
                        }
                        const auto two = pieces();
                        const auto gapF = call ("video_frame", R"({"seconds":3.5})"), earlyF = call ("video_frame", R"({"seconds":2.52})"), lateF = call ("video_frame", R"({"seconds":8.5})"), oldPlace = call ("video_frame", R"({"seconds":4.5})");
                        check (two.pieces.size() == 2 && gapF.contains ("gap between pieces") && earlyF.contains ("frame 13 of ramp.mp4") && lateF.contains ("frame 62 of ramp.mp4") && oldPlace.contains ("gap between pieces"),
                               "With the middle piece deleted and the last moved to 8 s: 2.52 s shows frame 13, 3.5 s and 4.5 s are gaps, 8.5 s shows frame 62 (the video's 2.5 s)");
                        // the Video window follows the pieces
                        vc.readState = [&] { VideoContent::State st; st.video = ramp; st.playhead = clock; st.playing = playing; const auto pc = VideoCuts::read (ed, in.seconds); st.cut = true; st.pieces = pc.pieces; st.slip = pc.slip; return st; };
                        vc.seek = [&] (double t) { clock = t; };
                        clock = 8.5; pump (100); for (int i = 0; i < 200 && vc.frameShown() != 62; ++i) pump (30);
                        const auto shownLate = vc.frameShown(); const double readLate = frameOf (vc.imageShown());
                        clock = 3.5; pump (150); const bool blackInGap = vc.frameShown() == -1 && ! vc.imageShown().isValid();
                        const auto sb2 = vc.stripBounds(); const double secsV = vc.getPlayer().facts().seconds;
                        const double toLate = vc.seekToStripX (sb2.getX() + juce::roundToInt (sb2.getWidth() * 2.5 / secsV)), toCut = vc.seekToStripX (sb2.getX() + juce::roundToInt (sb2.getWidth() * 1.5 / secsV));
                        clock = 2.96; vc.update(); vc.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)); const double stepIn = clock;      // frame 24 -> 25 would be in the cut-out second: steps along the timeline instead
                        check (shownLate == 62 && std::abs (readLate - 62.0) <= 1.2 && blackInGap && std::abs (toLate - 8.5) < 0.03 && toCut < 0 && std::abs (stepIn - 3.0) < 1.0e-6,
                               "Video window with a cut picture: 8.5 s shows frame 62, a gap shows nothing, a click on the strip at the video's 2.5 s goes to 8.5 s, a click on a cut-out moment goes nowhere and says so");
                        // the lane draws one block per piece
                        {
                            TimelineState ts; ts.viewStart = 0.0; ts.secondsPerPixel = 0.02;
                            VideoLane lane (*session, ts); lane.setBounds (0, 0, headerWidthNow() + 900, VideoLane::laneH); lane.setVisible (true);
                            for (int i = 0; i < 800 && ! (lane.getStrip().size() > 0 && lane.getStrip().finished()); ++i) { lane.refresh(); juce::Thread::sleep (40); }
                            lane.refresh();
                            const auto shotL = lane.createComponentSnapshot (lane.getLocalBounds(), true, 1.0f);
                            { const juce::File sp ("/tmp/main_video_lane_cut.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shotL, os); }
                            auto lanePx = [&] (double t) { return shotL.getPixelAt (headerWidthNow() + juce::roundToInt (t / 0.02), VideoLane::laneH / 2); };
                            const double f1 = ((double) lanePx (2.5).getGreen() - 4.0) / 2.5, f2 = ((double) lanePx (9.0).getGreen() - 4.0) / 2.5;
                            check (lane.picture().pieces.size() == 2 && lanePx (5.0) == Theme::bg() && lanePx (1.0) == Theme::bg() && std::abs (f1 - 12.0) <= 14.0 && std::abs (f2 - 75.0) <= 14.0 && f2 - f1 > 40.0,
                                   "Video lane with a cut picture: two blocks with a gap between; the first shows early frames (reads " + juce::String (f1, 0) + "), the moved one late frames (reads " + juce::String (f2, 0) + ")");
                        }
                        // export the cut picture and read the file back frame by frame
                        const auto cutFile = dir.getChildFile ("cut.mp4");
                        const auto ex = call ("video_export", "{\"picture\":\"source\",\"start_seconds\":0,\"end_seconds\":10,\"wait_seconds\":240,\"path\":" + asJson (cutFile) + "}");
                        const auto ci = Frame::probe (cutFile); juce::String ge, readBack; int rightX = 0, askedX = 0;
                        struct Want { double t; double frame; };     // frame -100 = black
                        for (auto w : { Want { 1.0, -100 }, Want { 2.02, 0 }, Want { 2.52, 13 }, Want { 2.94, 23 }, Want { 3.5, -100 }, Want { 7.0, -100 }, Want { 8.02, 50 }, Want { 8.5, 62 }, Want { 9.5, 87 } })
                        {
                            ++askedX; const auto img = ci.ok ? Frame::grabFrame (cutFile, ci, w.t, 64, ge, false) : juce::Image();
                            const int g = img.isValid() ? (int) img.getPixelAt (32, 32).getGreen() : -1; const double got = ((double) g - 4.0) / 2.5;
                            const bool ok = w.frame < -50 ? (g >= 0 && g <= 3) : std::abs (got - w.frame) <= 1.3;
                            if (ok) ++rightX; readBack << juce::String (w.t, 2) << (w.frame < -50 ? "=black" : "=" + juce::String (got, 1)) << (ok ? " " : "(WRONG) ");
                        }
                        check (ex.contains ("exported") && ci.ok && ci.width == 64 && ci.height == 64 && std::abs (ci.seconds - 10.0) < 0.2 && std::abs (ci.fps - 25.0) < 0.01 && rightX == askedX,
                               "video_export of the cut picture, read back: " + readBack.trim() + " - " + juce::String (ci.seconds, 2) + " s at " + juce::String (ci.fps, 0) + " fps");
                        // slip the picture against its sound, and undo
                        call ("video_move", R"({"by_seconds":0.2,"sound":false})");
                        const auto slipped = call ("video_frame", R"({"seconds":8.5})"); const double sndStill = VideoCuts::clips (ed).size() == 2 ? VideoCuts::clips (ed)[1]->getPosition().getStart().inSeconds() : -1.0;
                        ed.getUndoManager().undo();
                        const auto unslipped = call ("video_frame", R"({"seconds":8.5})");
                        check (slipped.contains ("frame 57 of ramp.mp4") && std::abs (sndStill - 8.0) < 1.0e-6 && unslipped.contains ("frame 62 of ramp.mp4"), "video_move sound false slips the picture 0.2 s later than its sound (8.5 s shows frame 57, the clips stay); undo puts it back");
                        // every piece of sound deleted: no picture, said plainly
                        for (int i = (int) VideoCuts::clips (ed).size(); --i >= 0;) call ("delete_clip", "{" + trk + ",\"clip\":1}");
                        const auto noneText = call ("video_cuts", "{}"); const auto noneEx = call ("video_export", "{\"picture\":\"source\",\"start_seconds\":0,\"end_seconds\":2,\"path\":" + asJson (dir.getChildFile ("none.mp4")) + "}", false);
                        check (pieces().pieces.empty() && noneText.contains ("no picture anywhere") && noneEx.contains ("no picture to export"), "With every clip of its sound deleted there is no picture, and video_cuts and video_export say so");
                        // quitting: Don't Save leaves the file alone (the question itself needs a person)
                        check (session->saveOnClose, "Quitting saves unless Don't Save is chosen at the question");
                    }
                    // ---- 1.33: titles and captions - seen in the window, burned into the export, caption files in and out
                    {
                        vc.readState = nullptr; vc.seek = nullptr;      // the window must not go on reading a session that is about to be replaced
                        call ("new_session", R"({"name":"Selftest Text"})");
                        call ("video_import", "{\"path\":" + asJson (ramp) + "}");
                        auto& ed = *session->edit;
                        const auto t1 = call ("video_text", R"({"action":"add","text":"HELLO MOTH","style":"title","start_seconds":1.0,"length_seconds":1.0})");
                        const auto t2 = call ("video_text", R"({"text":"these are the caption words","start_seconds":2.5,"end_seconds":3.5})");
                        const auto listed = call ("video_text", "{}");
                        check (t1.contains ("1. 1.00 to 2.00 s  title: HELLO MOTH") && t2.contains ("2. 2.50 to 3.50 s  caption") && listed.contains ("2 texts on the picture") && VideoText::read (ed).size() == 2,
                               "video_text add: a title from 1 to 2 s and a caption from 2.5 to 3.5 s are in the session's list");
                        vc.readState = [&] { VideoContent::State st; st.video = ramp; st.playhead = clock; st.playing = playing; const auto pc = VideoCuts::read (ed, in.seconds); st.cut = true; st.pieces = pc.pieces; st.slip = pc.slip; st.texts = VideoText::read (ed); return st; };
                        auto whiteIn = [] (const juce::Image& img, juce::Rectangle<int> r) { int n = 0; r = r.getIntersection (img.getBounds()); for (int y = r.getY(); y < r.getBottom(); ++y) for (int x = r.getX(); x < r.getRight(); ++x) { const auto px = img.getPixelAt (x, y); if (px.getRed() > 244 && px.getGreen() > 244 && px.getBlue() > 244) ++n; } return n; };
                        auto windowShot = [&] (double t) { clock = t; pump (120); for (int i = 0; i < 100 && vc.frameShown() != Frame::frameIndexAt (t, 25.0); ++i) pump (30); return vc.createComponentSnapshot (vc.getLocalBounds(), true, 1.0f); };
                        const auto scr = vc.screenBounds(); const auto topHalf = scr.withHeight (scr.getHeight() * 2 / 3), bottomThird = scr.withTrimmedTop (scr.getHeight() * 2 / 3);
                        const auto sTitle = windowShot (1.5), sNone = windowShot (0.5), sCap = windowShot (3.0);
                        { const juce::File sp ("/tmp/main_video_title.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (sTitle, os); }
                        { const juce::File sp ("/tmp/main_video_caption.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (sCap, os); }
                        const int wTitle = whiteIn (sTitle, topHalf), wNone = whiteIn (sNone, scr), wCapLow = whiteIn (sCap, bottomThird), wCapHigh = whiteIn (sCap, topHalf);
                        check (wTitle > 300 && wNone == 0 && wCapLow > 150 && wCapHigh == 0,
                               "Video window: the title shows in the middle only during its time and the caption at the bottom during its own (white pixels: title " + juce::String (wTitle) + ", no text " + juce::String (wNone) + ", caption low " + juce::String (wCapLow) + " / high " + juce::String (wCapHigh) + ")");
                        // export with the words burned in, read the file back
                        const auto tf = dir.getChildFile ("words.mp4");
                        const auto tex = call ("video_export", "{\"picture\":\"source\",\"width\":640,\"height\":360,\"text\":\"both\",\"start_seconds\":0,\"end_seconds\":4,\"wait_seconds\":240,\"path\":" + asJson (tf) + "}");
                        const auto ti = Frame::probe (tf); juce::String ge;
                        auto fileShot = [&] (double t) { return ti.ok ? Frame::grabFrame (tf, ti, t, 640, ge, false) : juce::Image(); };
                        const auto fTitle = fileShot (1.5), fNone = fileShot (0.5), fCap = fileShot (3.0), fAfter = fileShot (3.6), fEdge = fileShot (2.02);
                        const juce::Rectangle<int> whole (0, 0, 640, 360), upper (0, 0, 640, 240), lower (0, 240, 640, 120);
                        const int eTitle = whiteIn (fTitle, upper), eNone = whiteIn (fNone, whole), eCapLow = whiteIn (fCap, lower), eCapHigh = whiteIn (fCap, upper), eAfter = whiteIn (fAfter, whole), eEdge = whiteIn (fEdge, whole);
                        const double under = fCap.isValid() ? ((double) fCap.getPixelAt (320, 60).getGreen() - 4.0) / 2.5 : -100.0;
                        const auto srtFile = tf.withFileExtension ("srt"); const auto srtBack = SqText::parseSrt (srtFile.loadFileAsString().toStdString());
                        check (tex.contains ("exported") && tex.contains ("2 texts are burned into the picture") && ti.ok && ti.width == 640 && ti.height == 360 && eTitle > 200 && eNone == 0 && eCapLow > 100 && eCapHigh == 0 && eAfter == 0 && eEdge == 0 && std::abs (under - 75.0) <= 1.5,
                               "video_export burns the words in: read back, the title is there at 1.5 s, gone at 0.5 s and 2.02 s, the caption at the bottom at 3.0 s and gone at 3.6 s; the picture under them is still frame "
                               + juce::String (under, 1) + " (white pixels: " + juce::String (eTitle) + " / " + juce::String (eNone) + " / " + juce::String (eCapLow) + " / " + juce::String (eAfter) + ")");
                        check (srtBack.size() == 2 && srtBack[0].text == "HELLO MOTH" && std::abs (srtBack[0].start - 1.0) < 0.002 && srtBack[1].text == "these are the caption words" && std::abs (srtBack[1].end - 3.5) < 0.002,
                               "text both also writes the caption file beside the video, with the same words and times");
                        // caption file in, edit, remove, undo
                        const auto capIn = dir.getChildFile ("in.srt"); capIn.replaceWithText ("1\n00:00:00,500 --> 00:00:01,250\nfirst line\nsecond line\n\n2\n00:00:02,000 --> 00:00:02,750\n<i>last</i>\n");
                        const auto imp = call ("video_text", "{\"action\":\"import_srt\",\"offset_seconds\":10,\"path\":" + asJson (capIn) + "}");
                        const auto afterImp = VideoText::read (ed);
                        const auto edited = call ("video_text", R"({"action":"edit","number":3,"text":"changed","place":"top","start_seconds":12})");
                        const auto afterEdit = VideoText::read (ed);
                        const auto removed = call ("video_text", R"({"action":"remove","number":1})"); const size_t afterRemove = VideoText::read (ed).size();
                        ed.getUndoManager().undo(); const size_t afterUndo = VideoText::read (ed).size();
                        const auto badNum = call ("video_text", R"({"action":"remove","number":99})", false);
                        check (imp.contains ("Brought in 2 captions") && afterImp.size() == 4 && afterImp[2].text == "first line\nsecond line" && std::abs (afterImp[2].start - 10.5) < 1.0e-9 && afterImp[3].text == "last"
                               && afterEdit.size() == 4 && afterEdit[2].text == "changed" && afterEdit[2].place == SqText::top && std::abs (afterEdit[2].start - 12.0) < 1.0e-9 && std::abs (afterEdit[2].end - 12.75) < 1.0e-9
                               && removed.contains ("HELLO MOTH") && afterRemove == 3 && afterUndo == 4 && badNum.contains ("1 to 4"),
                               "video_text: a caption file comes in at 10 s (two lines kept, tags dropped); edit changes words, place and start keeping the length; remove and undo work; a wrong number is refused");
                        // too many to burn in is refused with the way out; a caption file still works
                        { juce::String many; for (int i = 0; i < 160; ++i) many << (i + 1) << "\n" << juce::String::fromUTF8 (SqText::srtTime (i * 0.02).c_str()) << " --> " << juce::String::fromUTF8 (SqText::srtTime (i * 0.02 + 0.02).c_str()) << "\nc" << i << "\n\n";
                          const auto manyFile = dir.getChildFile ("many.srt"); manyFile.replaceWithText (many); call ("video_text", "{\"action\":\"import_srt\",\"replace\":true,\"path\":" + asJson (manyFile) + "}"); }
                        const auto tooMany = call ("video_export", "{\"picture\":\"black\",\"start_seconds\":0,\"end_seconds\":4,\"path\":" + asJson (dir.getChildFile ("many.mp4")) + "}", false);
                        const auto wiped = call ("video_text", R"({"action":"remove","all":true})");
                        check (tooMany.contains ("up to 150") && tooMany.contains ("caption file") && wiped.contains ("Removed") && VideoText::read (ed).empty(), "more than 150 texts in one export is refused with the caption-file way out; remove all clears them");
                    }
                    // ---- 1.34: the picture's shape, chapters from markers, the poster
                    {
                        // a clip whose left half is dark (40) and right half bright (200), 128 x 64, 2 s, no sound
                        const auto halves = dir.getChildFile ("halves.mp4"), rawH = dir.getChildFile ("halves.rgb"); juce::String oh;
                        {
                            juce::FileOutputStream os (rawH); juce::HeapBlock<juce::uint8> fr (128 * 64 * 3);
                            for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) std::memset (fr + (y * 128 + x) * 3, x < 64 ? 40 : 200, 3);
                            for (int k = 0; k < 50; ++k) os.write (fr, 128 * 64 * 3);
                        }
                        Frame::run ({ ff.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "128x64", "-r", "25", "-i", rawH.getFullPathName(),
                                      "-c:v", Frame::videoEncoder(), "-pix_fmt", "yuv420p", "-an", halves.getFullPathName() }, oh);
                        vc.readState = nullptr; vc.seek = nullptr; vc.shapeTool = nullptr;      // (1.34.1) it read the replaced session from its timer: the Intel Mac run died here
                        call ("new_session", R"({"name":"Selftest Shape"})");
                        const auto hi = call ("video_import", "{\"path\":" + asJson (halves) + "}");
                        auto& ed = *session->edit;
                        juce::String ge;
                        auto shapedExport = [&] (const juce::String& shapeArgs, const juce::String& name)
                        {
                            call ("video_shape", shapeArgs);
                            const auto f = dir.getChildFile (name + ".mp4");
                            call ("video_export", "{\"picture\":\"source\",\"width\":128,\"height\":128,\"start_seconds\":0,\"end_seconds\":2,\"wait_seconds\":240,\"path\":" + asJson (f) + "}");
                            const auto pi = Frame::probe (f); return pi.ok && pi.width == 128 && pi.height == 128 ? Frame::grabFrame (f, pi, 1.0, 128, ge, false) : juce::Image();
                        };
                        auto lum = [] (const juce::Image& img, int x, int y) { return img.isValid() ? (int) img.getPixelAt (x, y).getGreen() : -1; };
                        const auto eLeft = shapedExport (R"({"shape":"square","fit":"fill","focus_x":0})", "left"), eRight = shapedExport (R"({"focus_x":1})", "right"), eMid = shapedExport (R"({"focus_x":0.5})", "mid"), eFit = shapedExport (R"({"fit":"fit"})", "fit");
                        auto dark = [] (int v) { return std::abs (v - 40) <= 6; }; auto bright = [] (int v) { return std::abs (v - 200) <= 6; };
                        check (hi.contains ("128 x 64") && hi.contains ("no sound") && dark (lum (eLeft, 32, 64)) && dark (lum (eLeft, 96, 64)) && bright (lum (eRight, 32, 64)) && bright (lum (eRight, 96, 64))
                               && dark (lum (eMid, 32, 64)) && bright (lum (eMid, 96, 64)) && lum (eFit, 64, 8) >= 0 && lum (eFit, 64, 8) <= 6 && lum (eFit, 64, 120) <= 6 && dark (lum (eFit, 32, 64)) && bright (lum (eFit, 96, 64)),
                               "video_shape square, read back from the exported files: focus left keeps the dark half (" + juce::String (lum (eLeft, 32, 64)) + ", " + juce::String (lum (eLeft, 96, 64)) + "), focus right the bright half ("
                               + juce::String (lum (eRight, 32, 64)) + ", " + juce::String (lum (eRight, 96, 64)) + "), the middle half of each, and fit shows the whole picture between black bars (" + juce::String (lum (eFit, 64, 8)) + ")");
                        // the size that comes with a shape, and the Video window drawing it
                        call ("video_shape", R"({"shape":"vertical","fit":"fill","focus_x":1})");
                        const auto vfile = dir.getChildFile ("vertical.mp4");
                        const auto vex = call ("video_export", "{\"picture\":\"source\",\"start_seconds\":0,\"end_seconds\":1,\"wait_seconds\":300,\"path\":" + asJson (vfile) + "}");
                        const auto vp = Frame::probe (vfile); const auto vimg = vp.ok ? Frame::grabFrame (vfile, vp, 0.5, 270, ge, false) : juce::Image();
                        const auto said = call ("video_shape", "{}");
                        check (vex.contains ("9:16, 1080 x 1920, filled") && vp.ok && vp.width == 1080 && vp.height == 1920 && bright (lum (vimg, 30, 240)) && bright (lum (vimg, 240, 240)) && said.contains ("9:16 (1080 x 1920") && said.contains ("100 % across"),
                               "a vertical shape exports 1080 x 1920 with no size given, filled from the right side of the picture (reads " + juce::String (lum (vimg, 30, 240)) + ")");
                        vc.readState = [&] { VideoContent::State st; st.video = halves; st.playhead = clock; st.playing = playing; st.texts = VideoText::read (ed); st.shape = VideoShape::read (ed); return st; };
                        vc.shapeTool = [&] (const juce::var& args) { return tools.call ("video_shape", args).text; };
                        clock = 0.5; pump (200); for (int i = 0; i < 200 && ! vc.imageShown().isValid(); ++i) pump (30);
                        const auto prV = vc.pictureRect(); const auto shotV = vc.createComponentSnapshot (vc.getLocalBounds(), true, 1.0f);
                        { const juce::File sp ("/tmp/main_video_vertical.png"); sp.deleteFile(); juce::FileOutputStream os (sp); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream (shotV, os); }
                        const int inL = lum (shotV, juce::roundToInt (prV.getX() + prV.getWidth() * 0.2f), juce::roundToInt (prV.getCentreY())), inR = lum (shotV, juce::roundToInt (prV.getX() + prV.getWidth() * 0.8f), juce::roundToInt (prV.getCentreY()));
                        const bool kr = vc.keyPressed (juce::KeyPress ('r', juce::ModifierKeys(), 'r')); const auto afterR = VideoShape::read (ed);
                        check (std::abs (prV.getWidth() / prV.getHeight() - 9.0f / 16.0f) < 0.01f && bright (inL) && bright (inR) && kr && afterR.shape.w == 1 && afterR.shape.h == 1,
                               "Video window: the picture is drawn 9:16 and filled from the right (reads " + juce::String (inL) + ", " + juce::String (inR) + "); the R key moves on to the next shape (square)");
                        // chapters from markers, read back from the file
                        call ("add_marker", R"({"name":"Verse","seconds":2.0})"); call ("add_marker", R"({"name":"Hook = big","seconds":4.0})");
                        const auto cfile = dir.getChildFile ("chapters.mp4");
                        const auto cex = call ("video_export", "{\"picture\":\"black\",\"width\":128,\"height\":128,\"start_seconds\":0,\"end_seconds\":6,\"wait_seconds\":240,\"path\":" + asJson (cfile) + "}");
                        juce::String facts; Frame::run ({ ff.getFullPathName(), "-hide_banner", "-i", cfile.getFullPathName() }, facts);
                        check (cex.contains ("3 chapters from the session's markers") && facts.contains ("start 0.000000, end 2.000000") && facts.contains ("start 2.000000, end 4.000000") && facts.contains ("start 4.000000, end 6.000000")
                               && facts.contains ("Start") && facts.contains ("Verse") && facts.contains ("Hook = big"),
                               "video_export puts the markers in the file as chapters; read back: Start 0-2 s, Verse 2-4 s, Hook = big 4-6 s");
                        const auto nfile = dir.getChildFile ("nochapters.mp4");
                        call ("video_export", "{\"picture\":\"black\",\"width\":128,\"height\":128,\"chapters\":false,\"start_seconds\":0,\"end_seconds\":6,\"wait_seconds\":240,\"path\":" + asJson (nfile) + "}");
                        juce::String facts2; Frame::run ({ ff.getFullPathName(), "-hide_banner", "-i", nfile.getFullPathName() }, facts2);
                        check (nfile.existsAsFile() && ! facts2.contains ("Chapter #"), "chapters false leaves them out");
                        // the poster: one moment exactly as exported
                        call ("video_shape", R"({"shape":"square","fit":"fill","focus_x":0})");
                        call ("video_text", R"({"text":"POSTER","style":"title","start_seconds":0.5,"length_seconds":1.0})");
                        const auto pfile = dir.getChildFile ("poster.mp4");
                        const auto pex = call ("video_export", "{\"picture\":\"source\",\"width\":256,\"height\":256,\"poster\":true,\"poster_seconds\":1.0,\"start_seconds\":0,\"end_seconds\":2,\"wait_seconds\":240,\"path\":" + asJson (pfile) + "}");
                        const auto poster = juce::ImageFileFormat::loadFrom (pfile.withFileExtension ("png"));
                        int whiteP = 0; if (poster.isValid()) for (int y = 0; y < poster.getHeight(); ++y) for (int x = 0; x < poster.getWidth(); ++x) { const auto px = poster.getPixelAt (x, y); if (px.getRed() > 244 && px.getGreen() > 244 && px.getBlue() > 244) ++whiteP; }
                        const auto pvid = Frame::probe (pfile); const auto pframe = pvid.ok ? Frame::grabFrame (pfile, pvid, 1.0, 256, ge, false) : juce::Image();
                        int whiteV = 0; if (pframe.isValid()) for (int y = 0; y < pframe.getHeight(); ++y) for (int x = 0; x < pframe.getWidth(); ++x) { const auto px = pframe.getPixelAt (x, y); if (px.getRed() > 244 && px.getGreen() > 244 && px.getBlue() > 244) ++whiteV; }
                        check (pex.contains ("Poster:") && poster.isValid() && poster.getWidth() == 256 && poster.getHeight() == 256 && dark (lum (poster, 20, 20)) && dark (lum (poster, 236, 236)) && whiteP > 150
                               && pframe.isValid() && dark (lum (pframe, 20, 20)) && std::abs (whiteP - whiteV) < juce::jmax (120, whiteP / 2),
                               "poster: a 256 x 256 picture of 1.0 s as exported - the left half of the video with the title on it (" + juce::String (whiteP) + " white pixels; the video's own frame there has " + juce::String (whiteV) + ")");
                        call ("video_shape", R"({"shape":"source"})"); const auto bad = call ("video_shape", R"({"shape":"banana"})", false);
                        check (! VideoShape::read (ed).active() && bad.contains ("shape is source"), "video_shape source goes back to the video's own shape; a shape that is not one is refused");
                    }
                    // ---- 1.35: the picture laid out on the beat grid; a silent video gets a silent clip to carry its picture
                    {
                        vc.readState = nullptr; vc.seek = nullptr; vc.shapeTool = nullptr;
                        call ("new_session", R"({"name":"Selftest Beat"})");
                        call ("video_import", "{\"path\":" + asJson (ramp) + "}");
                        auto& ed = *session->edit;
                        const double beat = ed.tempoSequence.toTime (te::BeatPosition::fromBeats (1.0)).inSeconds();
                        auto srcAt = [&] (double t) { const auto pc = VideoCuts::read (ed, in.seconds); const auto h = SqCut::lookup (pc.pieces, t, pc.slip, in.seconds); return h.shows() ? h.src : -1.0; };
                        auto count = [&] { return (int) VideoCuts::read (ed, in.seconds).pieces.size(); };
                        const int before = count();
                        const auto rev = call ("video_cut_to_beat", R"({"every_beats":1,"order":"reverse","start_seconds":0,"end_seconds":4})");
                        const int afterRev = count(); const double r1 = srcAt (0.26), r2 = srcAt (3.76), r3 = srcAt (2.1);
                        const auto revFrame = call ("video_frame", R"({"seconds":0.26})");
                        check (std::abs (beat - 0.5) < 1.0e-9 && before == 1 && afterRev >= 8 && afterRev <= 9 && rev.contains ("on the beat grid, one every 1 beat") && std::abs (r1 - 3.76) < 0.03 && std::abs (r2 - 0.26) < 0.03 && std::abs (r3 - 1.6) < 0.03
                               && (revFrame.contains ("frame 94 of ramp.mp4") || revFrame.contains ("frame 93 of ramp.mp4")),
                               "video_cut_to_beat reverse at 120 BPM: eight half-second pieces in the opposite order - 0.26 s shows the video's " + juce::String (r1, 2) + " s, 3.76 s shows " + juce::String (r2, 2) + " s, 2.1 s shows " + juce::String (r3, 2) + " s");
                        int undos = 0; while (count() != 1 && undos < 60) { ed.getUndoManager().undo(); ++undos; }
                        const double backTo = srcAt (2.1);
                        check (count() == 1 && std::abs (backTo - 2.1) < 1.0e-6 && undos >= 1, "undo brings the uncut picture back (" + juce::String (undos) + (undos == 1 ? " undo step" : " undo steps") + ")");
                        const auto spr = call ("video_cut_to_beat", R"({"every_beats":1,"order":"spread","start_seconds":0,"end_seconds":2})");
                        const double s0 = srcAt (0.1), s1 = srcAt (0.6), s3 = srcAt (1.9), rest = srcAt (3.0);
                        check (spr.contains ("4 pieces") && std::abs (s0 - 0.1) < 0.03 && std::abs (s1 - (1.1667 + 0.1)) < 0.04 && std::abs (s3 - 3.9) < 0.04 && std::abs (rest - 3.0) < 0.03,
                               "spread over the first 2 s: four pieces taken evenly through the whole video (0.6 s shows " + juce::String (s1, 2) + " s, 1.9 s shows " + juce::String (s3, 2) + " s); the picture after 2 s is untouched");
                        const auto shA = call ("video_cut_to_beat", R"({"every_beats":2,"order":"shuffle","seed":5,"start_seconds":0,"end_seconds":4,"mute":true})");
                        std::set<int> used; bool anyInPlace = false; for (int i = 0; i < 4; ++i) { const double sv = srcAt (i * 1.0 + 0.05); const int k = juce::roundToInt ((sv - 0.05) / 1.0); used.insert (k); if (k == i) anyInPlace = true; }
                        const auto trackOfClips = VideoCuts::clips (ed).empty() ? nullptr : VideoCuts::clips (ed)[0]->getTrack();
                        const auto bad = call ("video_cut_to_beat", R"({"order":"sideways"})", false);
                        check (shA.contains ("order: shuffle") && used.size() == 4 && ! anyInPlace && trackOfClips != nullptr && trackOfClips->isMuted (false) && bad.contains ("order is keep"),
                               "shuffle every 2 beats: the four one-second stretches are all used and none is in its own slot; mute silences the footage's track; a wrong order is refused");
                        // 1.36: the Cut button's action and the B key go through the same call
                        {
                            while (count() != 1 && ed.getUndoManager().canUndo()) ed.getUndoManager().undo();
                            const int clean = count();
                            vc.cutTool = [&] (const juce::var& args) { return tools.call ("video_cut_to_beat", args).text; };
                            vc.readState = [&] { VideoContent::State st; st.video = ramp; st.playhead = clock; st.playing = playing; const auto pc = VideoCuts::read (ed, in.seconds); st.cut = true; st.pieces = pc.pieces; st.slip = pc.slip; return st; };
                            clock = 0.5; pump (150);
                            const auto byMenu = vc.cutToBeat (2.0); const int afterMenu = count();
                            ed.getUndoManager().undo(); const int undone = count();
                            const bool keyOk = vc.keyPressed (juce::KeyPress ('b', juce::ModifierKeys(), 'b')); const int afterKey = count();
                            check (clean == 1 && byMenu.contains ("on the beat grid, one every 2 beats") && afterMenu >= 4 && afterMenu <= 5 && undone == 1 && keyOk && afterKey == afterMenu,
                                   "Video window: Cut on every 2 beats makes " + juce::String (afterMenu) + " pieces, one undo takes it back, and the B key repeats the same cut");
                            vc.readState = nullptr; vc.cutTool = nullptr;
                        }
                        // a video with no sound gets a silent clip, so its picture can be cut too
                        call ("new_session", R"({"name":"Selftest Silent"})");
                        const auto si = call ("video_import", "{\"path\":" + asJson (dir.getChildFile ("halves.mp4")) + "}");
                        auto& ed2 = *session->edit; const auto hin = Frame::probe (dir.getChildFile ("halves.mp4"));
                        const auto p0 = VideoCuts::read (ed2, hin.seconds);
                        const auto cutSilent = call ("video_cut_to_beat", R"({"every_beats":1,"order":"reverse","start_seconds":0,"end_seconds":2})");
                        const auto p1 = VideoCuts::read (ed2, hin.seconds);
                        const auto sfile = dir.getChildFile ("silentcut.mp4");
                        const auto sex = call ("video_export", "{\"picture\":\"source\",\"start_seconds\":0,\"end_seconds\":2,\"wait_seconds\":240,\"path\":" + asJson (sfile) + "}");
                        const auto sp = Frame::probe (sfile); juce::String ge2; const auto simg = sp.ok ? Frame::grabFrame (sfile, sp, 1.2, 128, ge2, false) : juce::Image();
                        check (si.contains ("silent clip on track") && p0.followsSound && p0.pieces.size() == 1 && std::abs (p0.pieces[0].length - hin.seconds) < 0.05 && cutSilent.contains ("4 pieces") && p1.pieces.size() == 4
                               && sex.contains ("exported") && sp.ok && sp.width == 128 && sp.height == 64 && simg.isValid() && std::abs ((int) simg.getPixelAt (20, 32).getGreen() - 40) <= 6 && std::abs ((int) simg.getPixelAt (108, 32).getGreen() - 200) <= 6,
                               "a video with no sound gets a silent clip that carries its picture: it can be cut to the beat (4 pieces) and the cut picture exports");
                    }
                    vc.getPlayer().stopStream();
                }
                dir.deleteRecursively();
            }
        }
        // ---- 1.37 Voltage: the triode stage as a plug-in (the circuit itself is checked against a circuit simulator in tests/tube_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage"})");
            SqVoltagePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltagePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage"), "Voltage is registered and inserts by name");
            if (vp != nullptr)
            {
                const double sr = 44100.0; const int N = 44100;
                struct Out { std::vector<float> l, r; };
                auto render = [&] (float driveDb, float mixPct, float amp, double hz, int unitSeed, int qualityIx = 0)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltagePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltagePlugin*> (twin.get()))
                    {
                        tw->unit = unitSeed; tw->quality->setParameter ((float) qualityIx, juce::dontSendNotification); tw->drive->setParameter (driveDb, juce::dontSendNotification); tw->mix->setParameter (mixPct, juce::dontSendNotification);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / sr); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                // a bin of exactly 1 kHz over 441 * 40 samples (40 Hz steps would not land; 1000 Hz * 17640 / 44100 = 400 cycles)
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 22050, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                const auto quiet = render (0.0f, 100.0f, 0.05f, 1000.0, 1234);
                const double inDb = 20 * std::log10 (0.05), lvl = bin (quiet.l, 1000.0);
                double corr = 0; for (int i = 22050; i < 22050 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.05 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - SqVoltagePlugin::delaySamples) / sr);
                check (std::abs (lvl - inDb) < 0.3 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage at a low level is level-matched (" + juce::String (lvl - inDb, 2) + " dB), the right way up and 96 samples late, as it reports");
                const auto hot = render (18.0f, 100.0f, 0.25f, 1000.0, 1234);
                const double h2 = bin (hot.l, 2000.0) - bin (hot.l, 1000.0), h3 = bin (hot.l, 3000.0) - bin (hot.l, 1000.0), comp = bin (hot.l, 1000.0) - 20 * std::log10 (0.25);
                const double h2quiet = bin (quiet.l, 2000.0) - bin (quiet.l, 1000.0);
                check (h2 > -30.0 && h2 > h2quiet + 15.0 && comp < -0.1 && comp > -6.0 && std::isfinite (h3),
                       "driven 18 dB harder the 2nd harmonic rises from " + juce::String (h2quiet, 1) + " to " + juce::String (h2, 1) + " dBc (3rd " + juce::String (h3, 1) + ") and the stage compresses by " + juce::String (-comp, 1) + " dB");
                const auto other = render (18.0f, 100.0f, 0.25f, 1000.0, 99999);
                const double dUnit = std::abs ((bin (other.l, 2000.0) - bin (other.l, 1000.0)) - h2), dLR = std::abs ((bin (hot.r, 2000.0) - bin (hot.r, 1000.0)) - h2);
                const auto again = render (18.0f, 100.0f, 0.25f, 1000.0, 1234);
                double same = 0; for (size_t i = 0; i < again.l.size() && i < hot.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - hot.l[i]));
                check (dUnit > 0.01 && dUnit < 3.0 && dLR > 0.01 && dLR < 3.0 && same < 1.0e-6,
                       "two units differ slightly (2nd harmonic " + juce::String (dUnit, 2) + " dB apart), left and right are two tubes (" + juce::String (dLR, 2) + " dB apart), and the same unit is the same every time");
                const auto dry = render (18.0f, 0.0f, 0.25f, 1000.0, 1234);
                double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - SqVoltagePlugin::delaySamples) / sr)));
                const auto hq = render (18.0f, 100.0f, 0.25f, 1000.0, 1234, 1);
                check (worstDry < 1.0e-5 && std::abs ((bin (hq.l, 2000.0) - bin (hq.l, 1000.0)) - h2) < 0.1, "Mix at 0 % is the input, delayed by the same 96 samples; 8x gives the same harmonics as 4x (within 0.1 dB)");
                // the display's numbers come from the same circuit: they must agree with what the audio path does
                {
                    const auto shown = SqVoltagePlugin::measure (1234, false, 45.0f, true, 18.0f, 0.25f);
                    const auto restful = SqVoltagePlugin::measure (1234, false, 45.0f, true, 0.0f, 0.1f);
                    check (std::abs (shown.harmonics[0] - h2) < 1.0 && std::abs (shown.harmonics[1] - h3) < 2.0 && shown.plateVolts > 150.0 && shown.plateVolts < 260.0 && shown.cathodeVolts > 0.8 && shown.cathodeVolts < 2.5
                           && shown.gainDb > 30.0 && shown.gainDb < 40.0 && std::abs (restful.levelDb) < 0.2 && ! restful.gridConducts && shown.wave.size() == 384,
                           "the face shows the circuit's own numbers: 2nd harmonic " + juce::String (shown.harmonics[0], 1) + " dB against " + juce::String (h2, 1) + " dB measured in the audio, plate " + juce::String (shown.plateVolts, 1)
                           + " V and cathode " + juce::String (shown.cathodeVolts, 2) + " V at rest, stage gain " + juce::String (shown.gainDb, 1) + " dB");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltage") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage","preset":"Pushed Hard"})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && pp.contains ("Pushed Hard") && std::abs (vp->drive->getCurrentValue() - 14.0f) < 0.01f && Edition::packOf ("sqmVoltage") == Edition::Pack::color && vp->unit.get() > 0,
                       "Voltage has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, and a preset sets Drive to 14 dB");
                // Quality is a control like the others (in 1.37 it could only be set in the session file)
                const auto qs = call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage","params":[{"param":"Quality","text":"8x"}]})");
                const bool q8 = juce::roundToInt (vp->quality->getCurrentValue()) == 1 && vp->quality->getCurrentValueAsString() == "8x";
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage","params":[{"param":"Quality","text":"16x"}]})");
                check (q8 && juce::roundToInt (vp->quality->getCurrentValue()) == 2 && vp->quality->getCurrentValueAsString() == "16x", "Voltage's Quality is a control the assistant can set: 8x, then 16x (" + qs.substring (0, 60) + ")");
            }
        }
        // ---- 1.38 / 1.39 Voltage Power: the output stage and its transformer (iron with saturation and hysteresis since 1.39) as a plug-in; the circuit is checked against a circuit simulator in tests/power_test.cpp
        {
            call ("new_session", R"({"name":"Selftest Voltage Power"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Power"})");
            SqVoltagePowerPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltagePowerPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Power"), "Voltage Power is registered and inserts by name");
            if (vp != nullptr)
            {
                const double sr = 44100.0; const int N = 44100;
                struct Out { std::vector<float> l, r; };
                auto render = [&] (float driveDb, float mixPct, float amp, double hz, int unitSeed, int qualityIx = 0, float ironPct = 70.0f, int tubeIx = 0)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltagePowerPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltagePowerPlugin*> (twin.get()))
                    {
                        tw->unit = unitSeed; tw->quality->setParameter ((float) qualityIx, juce::dontSendNotification); tw->drive->setParameter (driveDb, juce::dontSendNotification); tw->mix->setParameter (mixPct, juce::dontSendNotification);
                        tw->iron->setParameter (ironPct, juce::dontSendNotification); tw->tube->setParameter ((float) tubeIx, juce::dontSendNotification);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / sr); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 22050, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                const auto quiet = render (0.0f, 100.0f, 0.05f, 1000.0, 1234);
                const double inDb = 20 * std::log10 (0.05), lvl = bin (quiet.l, 1000.0);
                double corr = 0; for (int i = 22050; i < 22050 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.05 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - SqVoltagePowerPlugin::delaySamples) / sr);
                check (std::abs (lvl - inDb) < 0.3 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage Power at a low level is level-matched (" + juce::String (lvl - inDb, 2) + " dB), the right way up and 96 samples late, as it reports");
                const auto hot = render (12.0f, 100.0f, 0.5f, 1000.0, 1234);
                const double h2 = bin (hot.l, 2000.0) - bin (hot.l, 1000.0), h3 = bin (hot.l, 3000.0) - bin (hot.l, 1000.0), comp = bin (hot.l, 1000.0) - 20 * std::log10 (0.5);
                const double h3quiet = bin (quiet.l, 3000.0) - bin (quiet.l, 1000.0);
                check (h3 > -40.0 && h3 > h3quiet + 15.0 && comp < -0.1 && comp > -12.0 && std::isfinite (h2),
                       "driven hard the 3rd harmonic rises from " + juce::String (h3quiet, 1) + " to " + juce::String (h3, 1) + " dBc (2nd " + juce::String (h2, 1) + ") and the stage compresses by " + juce::String (-comp, 1) + " dB");
                const auto other = render (12.0f, 100.0f, 0.5f, 1000.0, 99999);
                const double dUnit = std::abs ((bin (other.l, 2000.0) - bin (other.l, 1000.0)) - h2), dLR = std::abs ((bin (hot.r, 2000.0) - bin (hot.r, 1000.0)) - h2);
                const auto again = render (12.0f, 100.0f, 0.5f, 1000.0, 1234);
                double same = 0; for (size_t i = 0; i < again.l.size() && i < hot.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - hot.l[i]));
                check (dUnit > 0.01 && dUnit < 6.0 && dLR > 0.01 && dLR < 6.0 && same < 1.0e-6,
                       "two units differ slightly (2nd harmonic " + juce::String (dUnit, 2) + " dB apart), left and right are two tubes (" + juce::String (dLR, 2) + " dB apart), and the same unit is the same every time");
                const auto dry = render (12.0f, 0.0f, 0.25f, 1000.0, 1234);
                double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - SqVoltagePowerPlugin::delaySamples) / sr)));
                const auto hq = render (12.0f, 100.0f, 0.5f, 1000.0, 1234, 1);
                check (worstDry < 1.0e-5 && std::abs ((bin (hq.l, 3000.0) - bin (hq.l, 1000.0)) - h3) < 0.1, "Mix at 0 % is the input, delayed by the same 96 samples; 8x gives the same harmonics as 4x (within 0.1 dB)");
                // Iron: the size of the transformer decides where the bass falls away
                const auto bigIron = render (0.0f, 100.0f, 0.05f, 50.0, 1234, 0, 100.0f), smallIron = render (0.0f, 100.0f, 0.05f, 50.0, 1234, 0, 0.0f);
                const double lossBig = bin (bigIron.l, 50.0) - inDb, lossSmall = bin (smallIron.l, 50.0) - inDb;
                check (lossBig > -1.5 && lossSmall < lossBig - 4.0 && lossSmall > -20.0, "Iron: at 50 Hz the largest transformer loses " + juce::String (-lossBig, 2) + " dB and the smallest " + juce::String (-lossSmall, 1) + " dB");
                // 1.39: the iron saturates - at 50 Hz a small transformer adds upper harmonics that a large one does not, at the same Drive
                {
                    auto upper = [&] (const std::vector<float>& y, double f0) { double e = 0; for (int k = 4; k <= 9; ++k) e += std::pow (10.0, bin (y, f0 * k) / 10.0); return 10 * std::log10 (e + 1e-30) - bin (y, f0); };
                    const auto big50 = render (0.0f, 100.0f, 0.5f, 50.0, 1234, 0, 100.0f), small50 = render (0.0f, 100.0f, 0.5f, 50.0, 1234, 0, 10.0f), big1k = render (0.0f, 100.0f, 0.5f, 1000.0, 1234, 0, 100.0f), small1k = render (0.0f, 100.0f, 0.5f, 1000.0, 1234, 0, 10.0f);
                    const double ub = upper (big50.l, 50.0), us = upper (small50.l, 50.0), us1k = upper (small1k.l, 1000.0), ub1k = upper (big1k.l, 1000.0);
                    check (us > ub + 6.0 && us1k < us - 10.0 && std::isfinite (us) && std::isfinite (ub1k),
                           "the iron saturates in the bass: 50 Hz through a small transformer has harmonics 4 to 9 at " + juce::String (us, 1) + " dBc, through a large one " + juce::String (ub, 1) + " dBc; at 1 kHz the small one is far cleaner (" + juce::String (us1k, 1) + " dBc; large " + juce::String (ub1k, 1) + ")");
                    const auto shownIron = SqVoltagePowerPlugin::measure (1234, 0, 45.0f, 40.0f, 70.0f, 0.0f, 0.1f);
                    check (shownIron.ironPercent > 25.0 && shownIron.ironPercent < 80.0, "the face shows where the iron rests: " + juce::String (juce::roundToInt (shownIron.ironPercent)) + " % of saturation, from the tube's steady current");
                }
                const auto heavy = render (12.0f, 100.0f, 0.5f, 1000.0, 1234, 0, 70.0f, 1);
                check (std::abs ((bin (heavy.l, 3000.0) - bin (heavy.l, 1000.0)) - h3) > 0.3, "the Heavy tube distorts differently from the Classic one at the same settings (3rd harmonic " + juce::String (bin (heavy.l, 3000.0) - bin (heavy.l, 1000.0), 1) + " against " + juce::String (h3, 1) + " dBc)");
                {
                    const auto shown = SqVoltagePowerPlugin::measure (1234, 0, 45.0f, 40.0f, 70.0f, 12.0f, 0.5f);
                    const auto restful = SqVoltagePowerPlugin::measure (1234, 0, 45.0f, 40.0f, 70.0f, 0.0f, 0.1f);
                    check (std::abs (shown.harmonics[0] - h2) < 1.0 && std::abs (shown.harmonics[1] - h3) < 1.0 && shown.plateMilliamps > 40.0 && shown.plateMilliamps < 130.0 && shown.cathodeVolts > 10.0 && shown.cathodeVolts < 40.0
                           && shown.plateWatts > 10.0 && shown.plateWatts < 45.0 && shown.gainDb > 20.0 && shown.gainDb < 35.0 && std::abs (restful.levelDb) < 0.2 && ! restful.gridConducts && shown.wave.size() == 384,
                           "the face shows the circuit's own numbers: 3rd harmonic " + juce::String (shown.harmonics[1], 1) + " dB against " + juce::String (h3, 1) + " dB measured in the audio, " + juce::String (juce::roundToInt (shown.plateMilliamps))
                           + " mA and " + juce::String (juce::roundToInt (shown.plateWatts)) + " W at rest, stage gain " + juce::String (shown.gainDb, 1) + " dB");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltagePower") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Power","preset":"Flat Out"})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && pp.contains ("Flat Out") && std::abs (vp->drive->getCurrentValue() - 14.0f) < 0.01f && juce::roundToInt (vp->tube->getCurrentValue()) == 2 && juce::roundToInt (vp->quality->getCurrentValue()) == 1
                       && Edition::packOf ("sqmVoltagePower") == Edition::Pack::color && vp->unit.get() > 0,
                       "Voltage Power has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, and a preset sets Drive 14 dB, the Bold tube and 8x");
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Power","params":[{"param":"Tube","text":"Heavy"},{"param":"Quality","text":"4x"}]})");
                check (juce::roundToInt (vp->tube->getCurrentValue()) == 1 && vp->tube->getCurrentValueAsString() == "Heavy" && juce::roundToInt (vp->quality->getCurrentValue()) == 0, "the assistant sets Tube and Quality by name (Heavy, 4x)");
            }
        }
        // ---- 1.40 Voltage Tape: the tape machine as a plug-in (its parts are checked against their references in tests/tape_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Tape"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Tape"})");
            SqVoltageTapePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageTapePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Tape"), "Voltage Tape is registered and inserts by name");
            if (vp != nullptr)
            {
                const double sr = 44100.0; const int N = 44100 * 2, lat = SqVoltageTapePlugin::delaySamplesAt (sr);
                struct Out { std::vector<float> l, r; };
                struct Set { float drive = 0, bias = 50, mix = 100, wow = 0, flutter = 0, bump = 0; int speed = 1, eq = 0, quality = 0, unit = 1234; };
                auto render = [&] (const Set& st, float amp, double hz)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageTapePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageTapePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->drive, st.drive); setp (tw->bias, st.bias); setp (tw->mix, st.mix); setp (tw->wow, st.wow); setp (tw->flutter, st.flutter);
                        setp (tw->bump, st.bump); setp (tw->speed, (float) st.speed); setp (tw->eq, (float) st.eq);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / sr); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 44100, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                Set base;
                const auto quiet = render (base, 0.05f, 1000.0);
                const double inDb = 20 * std::log10 (0.05), lvl = bin (quiet.l, 1000.0);
                double corr = 0; for (int i = 44100; i < 44100 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.05 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (lvl - inDb) < 0.25 && corr > 0 && std::abs (vp->getLatencySeconds() - lat / sr) < 0.25 * lat / sr && lat > 300,
                       "Voltage Tape at a low level is level-matched (" + juce::String (lvl - inDb, 2) + " dB), the right way up and " + juce::String (lat) + " samples late at 44.1 kHz, as it reports");
                Set hotSet = base; hotSet.drive = 14.0f; const auto hot = render (hotSet, 0.7f, 1000.0);
                const double h3 = bin (hot.l, 3000.0) - bin (hot.l, 1000.0), h2 = bin (hot.l, 2000.0) - bin (hot.l, 1000.0), comp = bin (hot.l, 1000.0) - 20 * std::log10 (0.7), h3quiet = bin (quiet.l, 3000.0) - bin (quiet.l, 1000.0);
                check (h3 > -30.0 && h3 > h3quiet + 15.0 && h2 < -70.0 && comp < -0.5 && comp > -14.0,
                       "driven hard the 3rd harmonic rises from " + juce::String (h3quiet, 1) + " to " + juce::String (h3, 1) + " dBc, the tape compresses by " + juce::String (-comp, 1) + " dB, and there is no 2nd harmonic (" + juce::String (juce::roundToInt (h2)) + " dBc): tape is symmetrical");
                Set noBias = base; noBias.bias = 0.0f; const auto dead = render (noBias, 0.05f, 1000.0);
                const double deadDrop = bin (dead.l, 1000.0) - lvl, deadH3 = bin (dead.l, 3000.0) - bin (dead.l, 1000.0);
                Set over = hotSet; over.bias = 100.0f; const auto clean = render (over, 0.7f, 1000.0);
                check (deadDrop < -8.0 && deadH3 > -40.0 && bin (clean.l, 3000.0) - bin (clean.l, 1000.0) < h3 - 6.0,
                       "Bias: with none a quiet tone all but vanishes (" + juce::String (deadDrop, 1) + " dB against the working range, 3rd harmonic " + juce::String (deadH3, 1) + " dBc); overbiased, the hard-driven tone is cleaner (" + juce::String (bin (clean.l, 3000.0) - bin (clean.l, 1000.0), 1) + " against " + juce::String (h3, 1) + " dBc)");
                Set slow = base; slow.speed = 0; Set fast = base; fast.speed = 2;
                const auto s20 = render (slow, 0.05f, 20000.0), f20 = render (fast, 0.05f, 20000.0), s10 = render (slow, 0.05f, 10000.0);
                check (bin (s20.l, 20000.0) < bin (f20.l, 20000.0) - 1.0 && std::abs (bin (s10.l, 10000.0) - inDb) < 0.6,
                       "Speed: 20 kHz comes back " + juce::String (bin (s20.l, 20000.0) - inDb, 2) + " dB at 7.5 in/s and " + juce::String (bin (f20.l, 20000.0) - inDb, 2) + " dB at 30 in/s; 10 kHz is level at the slow speed (" + juce::String (bin (s10.l, 10000.0) - inDb, 2) + " dB)");
                Set bumped = base; bumped.bump = 100.0f; const auto b55 = render (bumped, 0.05f, 55.0), n55 = render (base, 0.05f, 55.0);
                Set nabHot = base; nabHot.drive = 12.0f; Set iecHot = nabHot; iecHot.eq = 1; const auto nb = render (nabHot, 0.7f, 50.0), ib = render (iecHot, 0.7f, 50.0);
                check (bin (b55.l, 55.0) > bin (n55.l, 55.0) + 0.8 && (bin (nb.l, 150.0) - bin (nb.l, 50.0)) > (bin (ib.l, 150.0) - bin (ib.l, 50.0)) + 2.0,
                       "Head Bump lifts 55 Hz by " + juce::String (bin (b55.l, 55.0) - bin (n55.l, 55.0), 2) + " dB at 15 in/s; with NAB the bass goes onto the tape hotter, so 50 Hz distorts more than with IEC (3rd harmonic " + juce::String (bin (nb.l, 150.0) - bin (nb.l, 50.0), 1) + " against " + juce::String (bin (ib.l, 150.0) - bin (ib.l, 50.0), 1) + " dBc)");
                // wow and flutter: pitch wanders, loudness does not; one transport for both channels
                {
                    Set wf = base; wf.wow = 0.6f; wf.flutter = 0.3f; const auto wob = render (wf, 0.05f, 3000.0), still = render (base, 0.05f, 3000.0);
                    auto wander = [&] (const std::vector<float>& y) { std::complex<double> acc = 0; std::vector<double> ph; const int win = 96; for (int i = 0; i < (int) y.size(); ++i) { auto z = [&] (int k) { return (double) y[(size_t) k] * std::polar (1.0, -juce::MathConstants<double>::twoPi * 3000.0 * k / sr); }; acc += z (i); if (i >= win) acc -= z (i - win); if (i >= 8820 && i % 49 == 0) ph.push_back (std::arg (acc)); }
                                                              double peak = 0; for (size_t i = 1; i < ph.size(); ++i) { double d = ph[i] - ph[i - 1]; while (d > juce::MathConstants<double>::pi) d -= juce::MathConstants<double>::twoPi; while (d < -juce::MathConstants<double>::pi) d += juce::MathConstants<double>::twoPi; peak = std::max (peak, std::abs (d / juce::MathConstants<double>::twoPi * (sr / 49.0) / 3000.0)); } return peak * 100.0; };
                    auto rms = [] (const std::vector<float>& y) { double e = 0; for (size_t i = 44100; i < y.size(); ++i) e += (double) y[i] * y[i]; return 10 * std::log10 (e / (double) (y.size() - 44100) + 1e-30); };
                    const double w1 = wander (wob.l), w0 = wander (still.l), w2 = wander (wob.r);
                    check (w1 > 0.3 && w1 < 0.95 && w0 < 0.08 && w1 > 8.0 * w0 && std::abs (w1 - w2) < 0.02 && std::abs (rms (wob.l) - rms (still.l)) < 0.1,
                           "Wow 0.6 % + Flutter 0.3 %: a 3 kHz tone wanders by up to " + juce::String (w1, 2) + " % in pitch (still: " + juce::String (w0, 3) + " %), both channels alike (" + juce::String (w2, 2) + " %), level unchanged (" + juce::String (rms (wob.l) - rms (still.l), 3) + " dB)");
                    Set other = wf; other.unit = 99999; const auto wob2 = render (other, 0.05f, 3000.0); double diff = 0; for (size_t i = 44100; i < wob.l.size(); ++i) diff = std::max (diff, (double) std::abs (wob.l[i] - wob2.l[i]));
                    const auto again = render (wf, 0.05f, 3000.0); double same = 0; for (size_t i = 0; i < again.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - wob.l[i]));
                    check (diff > 0.005 && same < 1.0e-6, "another machine wanders its own way; the same machine wanders the same way every time");
                }
                Set dryS = hotSet; dryS.mix = 0.0f; const auto dry = render (dryS, 0.25f, 1000.0);
                double worstDry = 0; for (int i = 2000; i < 60000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - lat) / sr)));
                Set hq = hotSet; hq.quality = 1; const auto hot8 = render (hq, 0.7f, 1000.0);
                check (worstDry < 1.0e-5 && std::abs ((bin (hot8.l, 3000.0) - bin (hot8.l, 1000.0)) - h3) < 0.1, "Mix at 0 % is the input, delayed by the same " + juce::String (lat) + " samples; 8x gives the same harmonics as 4x (within 0.1 dB)");
                {
                    const auto shown = SqVoltageTapePlugin::measure (1234, 14.0f, 50.0f, 1, 0, 0.0f, 0.7f);
                    check (std::abs (shown.third - h3) < 1.0 && shown.biasState == 1 && shown.highUs == 50.0 && shown.lowUs == 3180.0 && shown.wave.size() == 384 && std::abs (shown.at10k) < 0.3 && shown.at20k < 1.5
                           && SqVoltageTapePlugin::measure (1234, 0.0f, 0.0f, 1, 0, 0.0f).biasState == 0 && SqVoltageTapePlugin::measure (1234, 0.0f, 100.0f, 2, 1, 0.0f).highUs == 17.5,
                           "the face shows the machine's own numbers: 3rd harmonic " + juce::String (shown.third, 1) + " dB against " + juce::String (h3, 1) + " dB measured in the audio, flux curve 3180 + 50 us, bias in its working range, 20 kHz at " + juce::String (shown.at20k, 2) + " dB");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageTape") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Tape","preset":"Worn Transport"})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && pp.contains ("Worn Transport") && std::abs (vp->wow->getCurrentValue() - 0.6f) < 0.001f && juce::roundToInt (vp->speed->getCurrentValue()) == 0
                       && Edition::packOf ("sqmVoltageTape") == Edition::Pack::color && vp->unit.get() > 0 && Skins::artFor ("voltagetape").isValid() && Skins::artFor ("voltage").isValid() && Skins::artFor ("voltagepower").isValid(),
                       "Voltage Tape has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, a preset sets Wow 0.6 % at 7.5 in/s, and all three Voltage plug-ins have their plate artwork");
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Tape","params":[{"param":"Speed","text":"30 in/s"},{"param":"EQ","text":"IEC"},{"param":"Quality","text":"8x"}]})");
                check (juce::roundToInt (vp->speed->getCurrentValue()) == 2 && vp->speed->getCurrentValueAsString() == "30 in/s" && vp->eq->getCurrentValue() >= 0.5f && juce::roundToInt (vp->quality->getCurrentValue()) == 1, "the assistant sets Speed, EQ and Quality by name (30 in/s, IEC, 8x)");
            }
        }
        // ---- 1.41 Voltage Console and Console Mode (the transformer is checked against a circuit simulator in tests/console_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Console"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Console"})");
            SqVoltageConsolePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageConsolePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Console"), "Voltage Console is registered and inserts by name");
            if (vp != nullptr)
            {
                const double sr = 44100.0; const int N = 44100 * 2;
                struct Out { std::vector<float> l, r; };
                struct Set { float drive = 0, iron = 50, leak = -120, mix = 100; int unit = 1234; };
                auto render = [&] (const Set& st, float ampL, float ampR, double hz)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageConsolePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageConsolePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; tw->drive->setParameter (st.drive, juce::dontSendNotification); tw->iron->setParameter (st.iron, juce::dontSendNotification);
                        tw->crosstalk->setParameter (st.leak, juce::dontSendNotification); tw->mix->setParameter (st.mix, juce::dontSendNotification);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / sr); buf.setSample (0, i, ampL * v); buf.setSample (1, i, ampR * v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 44100, M = 35280; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                Set base;
                const auto quiet = render (base, 0.05f, 0.05f, 1000.0); const double inDb = 20 * std::log10 (0.05), lvl = bin (quiet.l, 1000.0);
                double worstAlign = 0; for (int i = 44100; i < 44100 + 4410; ++i) worstAlign = std::max (worstAlign, std::abs ((double) quiet.l[(size_t) i] - 0.05 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / sr)));
                check (std::abs (lvl - inDb) < 0.05 && worstAlign < 0.05 * 0.02 && vp->getLatencySeconds() == 0.0,
                       "Voltage Console at a low level is level-matched (" + juce::String (lvl - inDb, 3) + " dB) and adds no delay: the output sits on the input sample for sample (largest difference " + juce::String (worstAlign / 0.05 * 100.0, 2) + " %)");
                Set hot = base; hot.drive = 9.0f; const auto b50 = render (hot, 0.9f, 0.9f, 50.0), b1k = render (hot, 0.9f, 0.9f, 1000.0);
                const double t50 = bin (b50.l, 150.0) - bin (b50.l, 50.0), t1k = bin (b1k.l, 3000.0) - bin (b1k.l, 1000.0);
                Set smallS = hot; smallS.iron = 5.0f; Set bigS = hot; bigS.iron = 95.0f; const auto s50 = render (smallS, 0.9f, 0.9f, 50.0), g50 = render (bigS, 0.9f, 0.9f, 50.0);
                const double ts = bin (s50.l, 150.0) - bin (s50.l, 50.0), tg = bin (g50.l, 150.0) - bin (g50.l, 50.0);
                check (t50 > -60.0 && t50 < -20.0 && t1k < t50 - 25.0 && ts > tg + 10.0,
                       "iron distorts the bass, not the middle: 3rd harmonic " + juce::String (t50, 1) + " dBc at 50 Hz against " + juce::String (t1k, 1) + " dBc at 1 kHz; a small transformer " + juce::String (ts, 1) + " dBc, a large one " + juce::String (tg, 1) + " dBc");
                Set leaky = base; leaky.leak = -80.0f; const auto x1 = render (leaky, 0.5f, 0.0f, 1000.0), x10 = render (leaky, 0.5f, 0.0f, 10000.0), x0 = render (base, 0.5f, 0.0f, 1000.0);
                const double c1 = bin (x1.r, 1000.0) - bin (x1.l, 1000.0), c10 = bin (x10.r, 10000.0) - bin (x10.l, 10000.0);
                double silent = 0; for (float v : x0.r) silent = std::max (silent, (double) std::abs (v));
                check (std::abs (c1 + 80.0) < 0.3 && c10 > c1 + 16.0 && c10 < c1 + 22.0 && silent == 0.0,
                       "Crosstalk at -80 dB: with only the left playing the right carries " + juce::String (c1, 1) + " dB at 1 kHz and " + juce::String (c10, 1) + " dB at 10 kHz; off, the right is exactly silent");
                Set dryS = hot; dryS.mix = 0.0f; const auto dry = render (dryS, 0.25f, 0.25f, 1000.0); double worstDry = 0; for (int i = 0; i < 60000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / sr)));
                Set other = hot; other.unit = 99999; const auto o50 = render (other, 0.9f, 0.9f, 50.0), again = render (hot, 0.9f, 0.9f, 50.0);
                double same = 0; for (size_t i = 0; i < again.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - b50.l[i]));
                const double dUnit = std::abs ((bin (o50.l, 150.0) - bin (o50.l, 50.0)) - t50), dLR = std::abs ((bin (b50.r, 150.0) - bin (b50.r, 50.0)) - t50);
                check (worstDry < 1.0e-6 && dUnit > 0.005 && dUnit < 4.0 && dLR > 0.005 && dLR < 4.0 && same < 1.0e-6,
                       "Mix at 0 % is the input itself; two units differ slightly (" + juce::String (dUnit, 2) + " dB in the 3rd harmonic), left and right are two transformers (" + juce::String (dLR, 2) + " dB), the same unit is the same every time");
                {
                    const auto shown = SqVoltageConsolePlugin::measure (1234, 50.0f, 9.0f);
                    check (std::abs (shown.third50 - t50) < 1.5 && shown.third1k < shown.third50 - 25.0 && shown.fluxPercent > 1.0 && shown.fluxPercent < 60.0 && shown.cornerHz > 0.5 && shown.cornerHz < 10.0 && shown.wave.size() == 384,
                           "the face shows the transformer's own numbers: 3rd harmonic at 50 Hz " + juce::String (shown.third50, 1) + " dB against " + juce::String (t50, 1) + " dB measured in the audio, iron driven to " + juce::String (shown.fluxPercent, 1) + " % of saturation, bass corner " + juce::String (shown.cornerHz, 1) + " Hz");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageConsole") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Console","preset":"Leaky Rails"})");
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Console","params":[{"param":"Crosstalk","text":"Off"}]})"); const bool offByName = vp->crosstalk->getCurrentValue() <= -119.0f && vp->crosstalk->getCurrentValueAsString() == "Off";
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && pp.contains ("Leaky Rails") && offByName && Edition::packOf ("sqmVoltageConsole") == Edition::Pack::color && Skins::artFor ("voltageconsole").isValid(),
                       "Voltage Console has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, and the assistant can switch Crosstalk off by name");
            }
            // Console Mode: the whole session in one step
            {
                call ("new_session", R"({"name":"Selftest Console Mode"})");
                call ("add_track", R"({"kind":"audio","name":"Drums"})"); call ("add_track", R"({"kind":"audio","name":"Bass"})");
                call ("insert_plugin", R"({"track":"Bass","plugin":"Voltage Console"})");      // one put there by hand: Console Mode must leave it alone
                const int tracks = session->getTracks().size();
                const auto before = call ("console_mode", R"({"action":"status"})");
                const auto on = call ("console_mode", R"({"action":"on","drive":3,"iron":40})"); auto st = ConsoleMode::status (*session);
                bool placedLast = true, settings = true;
                for (auto t : session->getTracks())
                {
                    auto inserts = Session::getInserts (t->pluginList, t); auto* c = ConsoleMode::placedOn (t->pluginList, "channel");
                    placedLast = placedLast && c != nullptr && ! inserts.isEmpty() && inserts.getLast() == c;
                    settings = settings && c != nullptr && std::abs (c->drive->getCurrentValue() - 3.0f) < 0.01f && std::abs (c->iron->getCurrentValue() - 40.0f) < 0.01f && c->crosstalk->getCurrentValue() <= -119.0f;
                }
                auto* bus = ConsoleMode::placedOn (session->edit->getMasterPluginList(), "bus"); auto masterPlugins = session->edit->getMasterPluginList().getPlugins();
                check (before.contains ("off") && st.complete() && st.tracks == tracks && tracks >= 3 && placedLast && settings && bus != nullptr && masterPlugins.getFirst() == bus && std::abs (bus->crosstalk->getCurrentValue() + 80.0f) < 0.01f && on.contains ("Placed " + juce::String (tracks + 1)),
                       "Console Mode on: " + juce::String (st.channels) + " of " + juce::String (st.tracks) + " tracks get a transformer as their last insert with the asked settings, the master gets the summing stage first with crosstalk at -80 dB (" + on.substring (0, 40) + "...)");
                const auto again = call ("console_mode", R"({"action":"on"})"); call ("add_track", R"({"kind":"audio","name":"Late Arrival"})");
                const bool incomplete = ! ConsoleMode::status (*session).complete(); const auto fill = call ("console_mode", R"({"action":"on"})");
                check (again.contains ("Nothing to add") && incomplete && fill.contains ("Placed 1") && ConsoleMode::status (*session).complete(), "turning it on twice adds nothing; a track added later gets its transformer when it is turned on again");
                int byHand = 0; if (auto tt = session->findTrack ("Bass")) for (auto pl : tt->pluginList) if (dynamic_cast<SqVoltageConsolePlugin*> (pl) != nullptr && ! pl->state.hasProperty (ConsoleMode::mark())) ++byHand;
                const auto off = call ("console_mode", R"({"action":"off"})"); int left = 0, byHandAfter = 0;
                for (auto t : session->getTracks()) for (auto pl : t->pluginList) if (dynamic_cast<SqVoltageConsolePlugin*> (pl) != nullptr) { ++left; if (! pl->state.hasProperty (ConsoleMode::mark())) ++byHandAfter; }
                check (byHand == 1 && ! ConsoleMode::status (*session).on() && left == 1 && byHandAfter == 1 && off.contains ("removed " + juce::String (tracks + 2)), "Console Mode off removes exactly the ones it placed (" + juce::String (tracks + 2) + ") and leaves the one inserted by hand");
                session->edit->getUndoManager().undo(); const bool back = ConsoleMode::status (*session).complete(); session->edit->getUndoManager().redo();
                check (back && ! ConsoleMode::status (*session).on(), "one undo brings the whole console back; redo takes it away again");
            }
        }
        // ---- 1.42 Voltage Clip: the diode clipper as a plug-in (the circuit is checked against a circuit simulator in tests/clip_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Clip"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Clip"})");
            SqVoltageClipPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageClipPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Clip") && vp != nullptr && juce::roundToInt (vp->quality->getCurrentValue()) == 1, "Voltage Clip is registered, inserts by name and starts at 8x");
            if (vp != nullptr)
            {
                const double sr = 44100.0; const int N = 44100, lat = SqVoltageClipPlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float drive = 0, tone = 90, mix = 100; int diodes = 0, sym = 0, quality = 1, unit = 1234; };
                auto render = [&] (const Set& st, float amp, double hz)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageClipPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageClipPlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->drive, st.drive); setp (tw->tone, st.tone); setp (tw->mix, st.mix); setp (tw->diodes, (float) st.diodes); setp (tw->symmetry, (float) st.sym);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (q + i) / sr); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 22050, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -juce::MathConstants<double>::twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peak = [] (const std::vector<float>& y) { float p = 0; for (size_t i = 8000; i < y.size(); ++i) p = std::max (p, std::abs (y[i])); return (double) p; };
                Set base;
                const auto quiet = render (base, 0.02f, 1000.0); const double inDb = 20 * std::log10 (0.02), lvl = bin (quiet.l, 1000.0);
                double corr = 0; for (int i = 22050; i < 22050 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.02 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (lvl - inDb) < 0.1 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage Clip passes a quiet tone at its own level (" + juce::String (lvl - inDb, 3) + " dB), the right way up and 96 samples late, as it reports");
                // the ceiling: however hard it is driven, the output stays at or under full scale
                double worstPeak = 0; juce::String peaks;
                for (float dr : { 0.0f, 12.0f, 30.0f }) for (int dk = 0; dk < 3; ++dk) for (int sy = 0; sy < 2; ++sy) { Set s2 = base; s2.drive = dr; s2.diodes = dk; s2.sym = sy; const double p = peak (render (s2, 1.0f, 220.0).l); worstPeak = std::max (worstPeak, p); if (dk == 0 && sy == 0) peaks += juce::String (20 * std::log10 (p), 2) + " "; }
                check (worstPeak <= 1.0 && worstPeak > 0.9, "the diodes hold under full scale: a full-scale tone at Drive 0, 12 and 30 dB through every diode pair, even and lopsided, peaks at most " + juce::String (20 * std::log10 (worstPeak), 3) + " dBFS (silicon: " + peaks.trim() + ")");
                Set hot = base; hot.drive = 12.0f; const auto h = render (hot, 0.7f, 1000.0);
                const double h3 = bin (h.l, 3000.0) - bin (h.l, 1000.0), h2 = bin (h.l, 2000.0) - bin (h.l, 1000.0);
                Set lop = hot; lop.sym = 1; const auto hl = render (lop, 0.7f, 1000.0); const double l2 = bin (hl.l, 2000.0) - bin (hl.l, 1000.0);
                check (h3 > -20.0 && h2 < -70.0 && l2 > -35.0, "driven 12 dB: 3rd harmonic " + juce::String (h3, 1) + " dBc and no 2nd (" + juce::String (juce::roundToInt (h2)) + " dBc) with an even pair; Lopsided brings the 2nd up to " + juce::String (l2, 1) + " dBc");
                Set softS = hot; softS.diodes = 1; Set sharpS = hot; sharpS.diodes = 2; const auto hs = render (softS, 0.7f, 1000.0), hh = render (sharpS, 0.7f, 1000.0);
                const double f5 = bin (h.l, 5000.0) - bin (h.l, 1000.0), s5 = bin (hs.l, 5000.0) - bin (hs.l, 1000.0), k5 = bin (hh.l, 5000.0) - bin (hh.l, 1000.0);
                check (std::abs (s5 - f5) > 0.3 && std::abs (k5 - f5) > 0.3 && std::abs (k5 - s5) > 0.3, "the three diode pairs clip differently at the same Drive: 5th harmonic " + juce::String (f5, 1) + " (silicon), " + juce::String (s5, 1) + " (soft), " + juce::String (k5, 1) + " dBc (sharp)");
                Set dark = base; dark.tone = 0.0f; const auto d10 = render (dark, 0.02f, 10000.0), b10 = render (base, 0.02f, 10000.0);
                check (bin (d10.l, 10000.0) < bin (b10.l, 10000.0) - 6.0, "Tone: 10 kHz comes back " + juce::String (bin (d10.l, 10000.0) - inDb, 1) + " dB at its darkest and " + juce::String (bin (b10.l, 10000.0) - inDb, 1) + " dB at the default");
                Set dryS = hot; dryS.mix = 0.0f; const auto dry = render (dryS, 0.25f, 1000.0); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (i - lat) / sr)));
                Set other = hot; other.unit = 99999; const auto ho = render (other, 0.7f, 1000.0), again = render (hot, 0.7f, 1000.0);
                double same = 0; for (size_t i = 0; i < again.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - h.l[i]));
                const double dUnit = std::abs ((bin (ho.l, 5000.0) - bin (ho.l, 1000.0)) - f5), dLR = std::abs ((bin (h.r, 5000.0) - bin (h.r, 1000.0)) - f5);
                Set hq = hot; hq.quality = 2; const auto h16 = render (hq, 0.7f, 1000.0);
                check (worstDry < 1.0e-5 && dUnit > 0.001 && dUnit < 3.0 && dLR > 0.001 && dLR < 3.0 && same < 1.0e-6 && std::abs ((bin (h16.l, 3000.0) - bin (h16.l, 1000.0)) - h3) < 0.2,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ slightly (" + juce::String (dUnit, 2) + " dB in the 5th harmonic), left and right are two pairs (" + juce::String (dLR, 2) + " dB), the same unit is the same every time, 16x gives the same harmonics as 8x");
                {
                    const auto shown = SqVoltageClipPlugin::measure (1234, 0, false, 90.0f, 12.0f, 0.7f);
                    check (std::abs (shown.harmonics[1] - h3) < 1.0 && shown.harmonics[0] < -100.0 && shown.kneeVolts > 0.4 && shown.kneeVolts < 0.6 && shown.ceilingVolts > shown.kneeVolts && shown.ceilingVolts < 1.0 && shown.peakDb <= 0.0 && shown.wave.size() == 384,
                           "the face shows the circuit's own numbers: 3rd harmonic " + juce::String (shown.harmonics[1], 1) + " dB against " + juce::String (h3, 1) + " dB measured in the audio, diodes start at " + juce::String (shown.kneeVolts, 2) + " V and hold at " + juce::String (shown.ceilingVolts, 2) + " V, peak " + juce::String (shown.peakDb, 1) + " dBFS");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageClip") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Clip","preset":"Bass Growl"})");
                const bool presetOk = pp.contains ("Bass Growl") && std::abs (vp->drive->getCurrentValue() - 12.0f) < 0.01f && juce::roundToInt (vp->diodes->getCurrentValue()) == 1 && vp->symmetry->getCurrentValue() >= 0.5f;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Clip","params":[{"param":"Diodes","text":"Sharp"},{"param":"Symmetry","text":"Even"},{"param":"Quality","text":"16x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && juce::roundToInt (vp->diodes->getCurrentValue()) == 2 && vp->diodes->getCurrentValueAsString() == "Sharp" && vp->symmetry->getCurrentValue() < 0.5f && juce::roundToInt (vp->quality->getCurrentValue()) == 2
                       && Edition::packOf ("sqmVoltageClip") == Edition::Pack::color && Skins::artFor ("voltageclip").isValid(),
                       "Voltage Clip has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets the soft lopsided pair, and the assistant sets Diodes, Symmetry and Quality by name");
            }
        }
        // ---- 1.43 Voltage Comp: the FET compressor as a plug-in (the circuit is checked against a circuit simulator in tests/fet_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Comp"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Comp"})");
            SqVoltageCompPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageCompPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Comp") && juce::roundToInt (vp->quality->getCurrentValue()) == 1, "Voltage Comp is registered, inserts by name and starts at 8x");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int N = 44100, lat = SqVoltageCompPlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float input = 0, attack = 50, release = 50, output = 0, mix = 100; int ratio = 0, quality = 1, unit = 1234; };
                auto render = [&] (const Set& st, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageCompPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageCompPlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->input, st.input); setp (tw->ratio, (float) st.ratio); setp (tw->attack, st.attack); setp (tw->release, st.release); setp (tw->output, st.output); setp (tw->mix, st.mix);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                auto both = [&] (const Set& st, float amp, double hz) { return render (st, tone (amp, hz), tone (amp, hz)); };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 22050, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peakIn = [&] (const std::vector<float>& y, double fromS, double toS) { float p = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) p = std::max (p, std::abs (y[(size_t) i])); return 20 * std::log10 ((double) p + 1e-15); };
                Set base;
                const auto quiet = both (base, 0.02f, 1000.0); const double inDb = 20 * std::log10 (0.02), lvl = bin (quiet.l, 1000.0);
                double corr = 0; for (int i = 22050; i < 22050 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.02 * std::sin (twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (lvl - inDb) < 0.5 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage Comp leaves a tone under its threshold alone (" + juce::String (lvl - inDb, 3) + " dB), the right way up and 96 samples late, as it reports");
                // above the threshold: a full-scale tone is turned down, more at each ratio step
                double red[4] = {}; juce::String reds; bool deeper = true;
                for (int ix = 0; ix < 4; ++ix) { Set s2 = base; s2.ratio = ix; red[ix] = bin (both (s2, 1.0f, 1000.0).l, 1000.0); reds += juce::String (red[ix], 1) + " "; if (ix > 0) deeper = deeper && red[ix] < red[ix - 1] - 0.2; }
                check (red[0] < -5.0 && red[0] > -12.0 && deeper && red[3] > -14.0, "a full-scale tone, 12 dB over the threshold, comes out at " + reds.trim() + " dBFS at 4, 8, 12 and 20 : 1: turned down, and further at each step");
                {
                    juce::String shownRatios; bool ok = true; const double want[4] = { 4, 8, 12, 20 }; SqVoltageCompPlugin::Reading first;
                    for (int ix = 0; ix < 4; ++ix) { const auto m = SqVoltageCompPlugin::measure (1234, ix, 0.0f); if (ix == 0) first = m; ok = ok && std::abs (m.ratio - want[ix]) < 0.15 * want[ix]; shownRatios += juce::String (m.ratio, 1) + " "; }
                    check (ok && std::abs (first.reductionAtFull - red[0]) < 1.0 && first.out[0] < first.out[8] && std::abs (first.out[0] - first.in[0]) < 0.5 && first.second < -40.0,
                           "the face shows the circuit's own numbers: ratios " + shownRatios.trim() + " : 1 for the steps named 4, 8, 12 and 20; a full-scale tone turned down " + juce::String (first.reductionAtFull, 1) + " dB against " + juce::String (red[0], 1) + " dB measured in the audio; its 2nd harmonic " + juce::String (first.second, 1) + " dB");
                }
                // Input pushes the signal into the fixed threshold; Output is plain gain afterwards
                Set pushed = base; pushed.input = 12.0f; Set louder = base; louder.output = 6.0f;
                const double q0 = bin (both (base, 0.25f, 1000.0).l, 1000.0), q12 = bin (both (pushed, 0.25f, 1000.0).l, 1000.0), o6 = bin (both (louder, 0.25f, 1000.0).l, 1000.0);
                check (q12 > q0 + 1.0 && q12 < q0 + 8.0 && std::abs (o6 - q0 - 6.0) < 0.05, "Input 12 dB up brings a -12 dBFS tone up only " + juce::String (q12 - q0, 1) + " dB (it is now compressing); Output 6 dB up is 6 dB up (" + juce::String (o6 - q0, 2) + ")");
                // attack: the first peaks of a sudden loud tone get through a slow attack and not a fast one
                Set slowA = base; slowA.attack = 0.0f; slowA.ratio = 3; Set fastA = slowA; fastA.attack = 100.0f;
                const double pSlow = peakIn (both (slowA, 1.0f, 1000.0).l, 0.0, 0.004), pFast = peakIn (both (fastA, 1.0f, 1000.0).l, 0.0, 0.004);
                // release: after a loud passage stops, a fast release lets a quiet tone back up at once; a slow one keeps it down
                auto drop = std::function<float (int)> ([=] (int i) { return (i < 17640 ? 1.0f : 0.05f) * (float) std::sin (twoPi * 1000.0 * i / sr); });
                Set fastR = base; fastR.release = 100.0f; fastR.ratio = 3; Set slowR = fastR; slowR.release = 0.0f;
                const double want = 20 * std::log10 (0.05), rFast = peakIn (render (fastR, drop, drop).l, 0.70, 0.75) - want, rSlow = peakIn (render (slowR, drop, drop).l, 0.70, 0.75) - want;
                check (pSlow > pFast + 0.4 && rFast > -1.0 && rSlow < rFast - 2.0, "Attack: the first peaks of a sudden full-scale tone reach " + juce::String (pSlow, 1) + " dBFS at 800 microseconds and " + juce::String (pFast, 1) + " at 20; Release: 0.3 s after a loud passage a quiet tone is back to " + juce::String (rFast, 1) + " dB at 50 ms and still " + juce::String (rSlow, 1) + " dB down at 1.1 s");
                // one sidechain: a loud left turns the quiet right down by the same amount
                const auto link = render (base, tone (1.0f, 1000.0), tone (0.02f, 700.0)); const double gl = bin (link.l, 1000.0), gr = bin (link.r, 700.0) - inDb;
                const auto solo = render (base, tone (0.0f, 1000.0), tone (1.0f, 700.0)); const double gs = bin (solo.r, 700.0);
                check (gl < -5.0 && std::abs (gl - gr) < 0.7 && gs < -5.0, "both channels share one sidechain: a full-scale left (" + juce::String (gl, 2) + " dB) takes a quiet right down with it (" + juce::String (gr, 2) + " dB), and a loud right on its own is compressed too (" + juce::String (gs, 2) + " dB)");
                Set dryS = base; dryS.mix = 0.0f; dryS.input = 12.0f; const auto dry = both (dryS, 0.25f, 1000.0); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = base; other.unit = 99999; Set hq = base; hq.quality = 2; const auto a1 = both (base, 1.0f, 1000.0), a2 = both (base, 1.0f, 1000.0);
                double same = 0; for (size_t i = 0; i < a1.l.size(); ++i) same = std::max (same, (double) std::abs (a1.l[i] - a2.l[i]));
                const double dUnit = std::abs (bin (both (other, 1.0f, 1000.0).l, 1000.0) - red[0]), d16 = std::abs (bin (both (hq, 1.0f, 1000.0).l, 1000.0) - red[0]);
                check (worstDry < 1.0e-5 && dUnit > 0.001 && dUnit < 3.0 && same < 1.0e-6 && d16 < 0.3,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ slightly (" + juce::String (dUnit, 2) + " dB of reduction), the same unit is the same every time, 16x compresses as 8x does (" + juce::String (d16, 3) + " dB apart)");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageComp") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Comp","preset":"Ad-Lib Squeeze"})");
                const bool presetOk = pp.contains ("Ad-Lib Squeeze") && std::abs (vp->input->getCurrentValue() - 20.0f) < 0.01f && juce::roundToInt (vp->ratio->getCurrentValue()) == 2;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Comp","params":[{"param":"Ratio","text":"20 : 1"},{"param":"Attack","value":100},{"param":"Quality","text":"16x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && juce::roundToInt (vp->ratio->getCurrentValue()) == 3 && vp->ratio->getCurrentValueAsString() == "20 : 1" && vp->attack->getCurrentValueAsString() == "20 us" && juce::roundToInt (vp->quality->getCurrentValue()) == 2
                       && Edition::packOf ("sqmVoltageComp") == Edition::Pack::color && Skins::artFor ("voltagecomp").isValid(),
                       "Voltage Comp has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets Input and Ratio, and the assistant sets Ratio and Quality by name and Attack to its fastest (shown as 20 us)");
            }
        }
        // ---- 1.44 Voltage Opto: the optical compressor as a plug-in (the cell and its loop are checked against a circuit simulator in tests/opto_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Opto"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Opto"})");
            SqVoltageOptoPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageOptoPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Opto") && juce::roundToInt (vp->quality->getCurrentValue()) == 0 && vp->mode->getCurrentValueAsString() == "Compress", "Voltage Opto is registered, inserts by name and starts at 4x in Compress");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int lat = SqVoltageOptoPlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float reduce = 60, gain = 0, mix = 100; int mode = 0, quality = 0, unit = 1234; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageOptoPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageOptoPlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->reduce, st.reduce); setp (tw->gain, st.gain); setp (tw->mode, (float) st.mode); setp (tw->mix, st.mix);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                auto both = [&] (const Set& st, float amp, double hz, int N = 66150) { return render (st, N, tone (amp, hz), tone (amp, hz)); };
                // level of a tone over the last 0.2 s of a render
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 8820, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peakIn = [&] (const std::vector<float>& y, double fromS, double toS) { float p = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) p = std::max (p, std::abs (y[(size_t) i])); return 20 * std::log10 ((double) p + 1e-15); };
                Set base;
                Set offS = base; offS.reduce = 0.0f; const auto idle = both (offS, 0.5f, 1000.0, 22050); const double inDb = 20 * std::log10 (0.5), idleDb = lvl (idle.l, 1000.0) - inDb;
                double corr = 0; for (int i = 11025; i < 11025 + 4410; ++i) corr += (double) idle.l[(size_t) i] * 0.5 * std::sin (twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (idleDb + 0.043) < 0.01 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "with Reduction at 0 it is only the dark cell: a tone comes back " + juce::String (idleDb, 3) + " dB (the dark resistance's small loss), the right way up and 96 samples late, as it reports");
                const double full = lvl (both (base, 1.0f, 1000.0).l, 1000.0), fullLimit = [&] { Set s2 = base; s2.mode = 1; return lvl (both (s2, 1.0f, 1000.0).l, 1000.0); }();
                const double quietC = lvl (both (base, 0.25f, 1000.0).l, 1000.0), quietL = [&] { Set s2 = base; s2.mode = 1; return lvl (both (s2, 0.25f, 1000.0).l, 1000.0); }();
                const double ratioC = 12.04 / (full - quietC), ratioL = 12.04 / (fullLimit - quietL);
                check (full < -4.0 && full > -14.0 && ratioC > 1.8 && ratioC < 5.0 && ratioL > 6.0 && ratioL > 2.0 * ratioC,
                       "Reduction 60: a full-scale tone comes out at " + juce::String (full, 1) + " dBFS in Compress and " + juce::String (fullLimit, 1) + " in Limit; over the top 12 dB Compress measures " + juce::String (ratioC, 1) + " : 1 and Limit " + juce::String (ratioL, 1) + " : 1");
                {
                    const auto shown = SqVoltageOptoPlugin::measure (1234, false, 60.0f), shownL = SqVoltageOptoPlugin::measure (1234, true, 60.0f);
                    check (std::abs (shown.reductionAtFull - full) < 0.7 && std::abs (shown.ratio - ratioC) < 0.6 && shownL.ratio > 6.0 && shown.attackMs >= 4 && shown.attackMs <= 20 && shown.halfReleaseMs >= 40 && shown.halfReleaseMs <= 80 && shown.backSeconds > 1.0 && shown.backSeconds < 15.0
                           && std::abs (shown.out[0] - shown.in[0]) < 0.2,
                           "the face shows the cell's own numbers: a full-scale tone at " + juce::String (shown.reductionAtFull, 1) + " dBFS against " + juce::String (full, 1) + " measured in the audio, ratio " + juce::String (shown.ratio, 1) + " : 1 (Limit " + juce::String (shownL.ratio, 1)
                           + "), attack " + juce::String (juce::roundToInt (shown.attackMs)) + " ms, half released in " + juce::String (juce::roundToInt (shown.halfReleaseMs)) + " ms, all back in " + juce::String (shown.backSeconds, 1) + " s");
                }
                // the cell's memory: after a loud passage stops, a quiet tone comes back quickly at first, then slowly - and more slowly after a long passage
                auto after = [&] (double loudS) { return std::function<float (int)> ([=] (int i) { return (i < (int) (loudS * sr) ? 1.0f : 0.02f) * (float) std::sin (twoPi * 1000.0 * i / sr); }); };
                const double quietDb = 20 * std::log10 (0.02);
                const auto longP = render (base, (int) (3.2 * sr), after (2.0), after (2.0)), shortP = render (base, (int) (1.3 * sr), after (0.1), after (0.1));
                const double atEnd = peakIn (longP.l, 1.9, 1.99) - (full > -100 ? 0.0 : 0.0), g80 = peakIn (longP.l, 2.08, 2.1) - quietDb, g900 = peakIn (longP.l, 2.9, 2.95) - quietDb, s900 = peakIn (shortP.l, 1.0, 1.05) - quietDb;
                check (atEnd < -4.0 && g80 > 0.75 * atEnd && g80 < -1.0 && g900 < -0.8 && g900 > g80 && s900 > g900 + 0.5 && s900 > -1.0,
                       "it lets go fast, then slowly: after 2 s at " + juce::String (atEnd, 1) + " dB a quiet tone is " + juce::String (g80, 1) + " dB down 0.1 s later and still " + juce::String (g900, 1) + " dB down after 0.9 s; after only 0.1 s of loud it is " + juce::String (s900, 1) + " dB down at 0.9 s - the cell remembers how long it was lit");
                const auto slowHit = both (base, 1.0f, 1000.0, 8820); const double first = peakIn (slowHit.l, 0.0, 0.004), later = peakIn (slowHit.l, 0.1, 0.19);
                check (first > later + 2.0, "it takes a few milliseconds to grab: the first peaks of a sudden full-scale tone reach " + juce::String (first, 1) + " dBFS before it settles at " + juce::String (later, 1));
                const auto link = render (base, 66150, tone (1.0f, 1000.0), tone (0.02f, 700.0)); const double gl = lvl (link.l, 1000.0), gr = lvl (link.r, 700.0) - quietDb;
                const auto solo = render (base, 66150, tone (0.0f, 1000.0), tone (1.0f, 700.0)); const double gs = lvl (solo.r, 700.0);
                check (gl < -4.0 && std::abs (gl - gr) < 0.5 && gs < -4.0, "both channels sit under one panel: a full-scale left (" + juce::String (gl, 2) + " dB) takes a quiet right down with it (" + juce::String (gr, 2) + " dB), and a loud right on its own is compressed too (" + juce::String (gs, 2) + " dB)");
                Set dryS = base; dryS.mix = 0.0f; const auto dry = both (dryS, 0.5f, 1000.0, 33075); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.5 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = base; other.unit = 99999; Set hq = base; hq.quality = 2; Set loud = base; loud.gain = 6.0f; const auto a1 = both (base, 1.0f, 1000.0, 22050), a2 = both (base, 1.0f, 1000.0, 22050);
                double same = 0; for (size_t i = 0; i < a1.l.size(); ++i) same = std::max (same, (double) std::abs (a1.l[i] - a2.l[i]));
                const double dUnit = std::abs (lvl (both (other, 1.0f, 1000.0).l, 1000.0) - full), d16 = std::abs (lvl (both (hq, 1.0f, 1000.0).l, 1000.0) - full), up6 = lvl (both (loud, 1.0f, 1000.0).l, 1000.0) - full;
                check (worstDry < 1.0e-5 && dUnit > 0.01 && dUnit < 3.0 && same < 1.0e-6 && d16 < 0.1 && std::abs (up6 - 6.0) < 0.02,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ (" + juce::String (dUnit, 2) + " dB of reduction - cells vary), the same unit is the same every time, 16x compresses as 4x does (" + juce::String (d16, 3) + " dB apart), Gain 6 dB up is " + juce::String (up6, 2) + " dB up");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageOpto") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Opto","preset":"808 Hold"})");
                const bool presetOk = pp.contains ("808 Hold") && std::abs (vp->reduce->getCurrentValue() - 55.0f) < 0.01f && vp->mode->getCurrentValueAsString() == "Limit";
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Opto","params":[{"param":"Mode","text":"Compress"},{"param":"Reduction","text":"70"},{"param":"Quality","text":"8x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->mode->getCurrentValueAsString() == "Compress" && std::abs (vp->reduce->getCurrentValue() - 70.0f) < 0.01f && juce::roundToInt (vp->quality->getCurrentValue()) == 1
                       && Edition::packOf ("sqmVoltageOpto") == Edition::Pack::color && Skins::artFor ("voltageopto").isValid(),
                       "Voltage Opto has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets Reduction and Limit, and the assistant sets Mode, Reduction and Quality by name");
            }
        }
        // ---- 1.45 Voltage Mu: the variable-mu compressor as a plug-in (the tubes and the loop are checked against a circuit simulator in tests/mu_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Mu"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Mu"})");
            SqVoltageMuPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageMuPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Mu") && juce::roundToInt (vp->quality->getCurrentValue()) == 0 && vp->mode->getCurrentValueAsString() == "Compress" && vp->recovery->getCurrentValueAsString() == "0.6 s", "Voltage Mu is registered, inserts by name and starts at 4x in Compress with a 0.6 s recovery");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int lat = SqVoltageMuPlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float input = 0, threshold = -14, attack = 50, output = 0, mix = 100; int recovery = 2, mode = 0, quality = 0, unit = 1234; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageMuPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageMuPlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->input, st.input); setp (tw->threshold, st.threshold); setp (tw->attack, st.attack); setp (tw->recovery, (float) st.recovery); setp (tw->mode, (float) st.mode); setp (tw->output, st.output); setp (tw->mix, st.mix);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                auto both = [&] (const Set& st, float amp, double hz, int N = 132300) { return render (st, N, tone (amp, hz), tone (amp, hz)); };
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 8820, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peakIn = [&] (const std::vector<float>& y, double fromS, double toS) { float p = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) p = std::max (p, std::abs (y[(size_t) i])); return 20 * std::log10 ((double) p + 1e-15); };
                Set base; Set lim = base; lim.mode = 1;
                const auto quiet = both (base, 0.05f, 1000.0, 22050); const double quietDb = 20 * std::log10 (0.05), q0 = lvl (quiet.l, 1000.0) - quietDb;
                double corr = 0; for (int i = 11025; i < 11025 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.05 * std::sin (twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (q0) < 0.05 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage Mu leaves a tone under its threshold alone (" + juce::String (q0, 3) + " dB), the right way up and 96 samples late, as it reports");
                const double fullC = lvl (both (base, 1.0f, 1000.0).l, 1000.0), fullL = lvl (both (lim, 1.0f, 1000.0).l, 1000.0);
                const double atC = lvl (both (base, 0.2f, 1000.0).l, 1000.0), upC = lvl (both (base, 0.794f, 1000.0).l, 1000.0), atL = lvl (both (lim, 0.2f, 1000.0).l, 1000.0), upL = lvl (both (lim, 0.794f, 1000.0).l, 1000.0);
                const double ratioC = 11.98 / (upC - atC), ratioL = 11.98 / (upL - atL);
                check (std::abs (ratioC - 1.5) < 0.15 && std::abs (ratioL - 4.0) < 0.5 && fullL < fullC - 3.0 && fullC < -3.0,
                       "over the first 12 dB above the threshold Compress measures " + juce::String (ratioC, 2) + " : 1 and Limit " + juce::String (ratioL, 2) + " : 1; a full-scale tone comes out at " + juce::String (fullC, 1) + " and " + juce::String (fullL, 1) + " dBFS");
                {
                    const auto shown = SqVoltageMuPlugin::measure (1234, false, -14.0f, 0.0f, 50.0f, 2), shownL = SqVoltageMuPlugin::measure (1234, true, -14.0f, 0.0f, 50.0f, 2);
                    check (std::abs (shown.reductionAtFull - fullC) < 0.5 && std::abs (shownL.reductionAtFull - fullL) < 0.5 && std::abs (shown.ratio - ratioC) < 0.2 && std::abs (shownL.ratio - ratioL) < 0.5 && shownL.third < -55.0 && shownL.third > -95.0 && shownL.gridsAtFull < -4.0 && shownL.gridsAtFull > -13.5
                           && std::abs (shown.out[0] - shown.in[0]) < 0.05,
                           "the face works the same numbers out from the tubes without running the audio: a full-scale tone at " + juce::String (shown.reductionAtFull, 1) + " dB (audio " + juce::String (fullC, 1) + ") and in Limit " + juce::String (shownL.reductionAtFull, 1) + " dB (audio " + juce::String (fullL, 1)
                           + "), ratios " + juce::String (shown.ratio, 2) + " and " + juce::String (shownL.ratio, 2) + " : 1, grids at " + juce::String (shownL.gridsAtFull, 1) + " V, 3rd harmonic " + juce::String (shownL.third, 0) + " dB");
                }
                // recovery: after a loud passage stops, a quiet tone is back quickly at 0.2 s and held down at 8 s
                auto drop = std::function<float (int)> ([=] (int i) { return (i < (int) (1.5 * sr) ? 1.0f : 0.05f) * (float) std::sin (twoPi * 1000.0 * i / sr); });
                Set fastR = lim; fastR.recovery = 0; Set slowR = lim; slowR.recovery = 4;
                const double rFast = peakIn (render (fastR, 88200, drop, drop).l, 1.8, 1.85) - quietDb, rSlow = peakIn (render (slowR, 88200, drop, drop).l, 1.8, 1.85) - quietDb;
                Set fastA = lim; fastA.attack = 25.0f; Set slowA = lim; slowA.attack = 70.0f;
                const double pFast = peakIn (both (fastA, 1.0f, 1000.0, 4410).l, 0.005, 0.015), pSlow = peakIn (both (slowA, 1.0f, 1000.0, 4410).l, 0.005, 0.015), pFirst = peakIn (both (fastA, 1.0f, 1000.0, 4410).l, 0.0, 0.004);
                check (rFast > -3.0 && rSlow < rFast - 5.0 && pSlow > pFast + 1.5 && pFirst > pFast + 3.0,
                       "Recovery: 0.3 s after a loud passage a quiet tone is " + juce::String (rFast, 1) + " dB down at 0.2 s and " + juce::String (rSlow, 1) + " dB down at 8 s; Attack: a sudden full-scale tone starts at " + juce::String (pFirst, 1) + " dBFS and 5 to 15 ms in has come down to "
                       + juce::String (pFast, 1) + " at 25 ms and " + juce::String (pSlow, 1) + " at 70 ms - it is slow on purpose");
                // Input pushes into the threshold and drives the tubes harder
                Set pushed = lim; pushed.input = 6.0f; const double in6 = lvl (both (pushed, 0.2f, 1000.0).l, 1000.0) - atL;
                Set clean = base; clean.threshold = 0.0f; Set hot = clean; hot.input = 12.0f; const auto yc = both (clean, 0.2f, 1000.0, 44100), yh = both (hot, 0.2f, 1000.0, 44100);
                const double h3c = lvl (yc.l, 3000.0) - lvl (yc.l, 1000.0), h3h = lvl (yh.l, 3000.0) - lvl (yh.l, 1000.0), hotGain = lvl (yh.l, 1000.0) - lvl (yc.l, 1000.0);
                Set loud = base; loud.output = 6.0f; const double up6 = lvl (both (loud, 1.0f, 1000.0).l, 1000.0) - fullC;
                check (in6 > 0.5 && in6 < 4.0 && h3h > h3c + 15.0 && h3h < -60.0 && std::abs (hotGain - 12.0) < 0.1 && std::abs (up6 - 6.0) < 0.02,
                       "Input 6 dB up brings a tone at the threshold up only " + juce::String (in6, 1) + " dB in Limit; with no compression Input 12 dB up is " + juce::String (hotGain, 2) + " dB up and the tubes' 3rd harmonic rises from " + juce::String (h3c, 0) + " to " + juce::String (h3h, 0) + " dB; Output 6 dB up is " + juce::String (up6, 2) + " dB up");
                const auto link = render (lim, 132300, tone (1.0f, 1000.0), tone (0.01f, 700.0)); const double gl = lvl (link.l, 1000.0), gr = lvl (link.r, 700.0) + 40.0;
                const auto solo = render (lim, 132300, tone (0.0f, 1000.0), tone (1.0f, 700.0)); const double gs = lvl (solo.r, 700.0);
                check (gl < -8.0 && std::abs (gl - gr) < 0.5 && gs < -8.0, "both channels share one sidechain: a full-scale left (" + juce::String (gl, 2) + " dB) takes a quiet right down with it (" + juce::String (gr, 2) + " dB), and a loud right on its own is compressed too (" + juce::String (gs, 2) + " dB)");
                Set dryS = lim; dryS.mix = 0.0f; const auto dry = both (dryS, 0.5f, 1000.0, 33075); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.5 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = lim; other.unit = 99999; Set hq = lim; hq.quality = 2; const auto a1 = both (lim, 1.0f, 1000.0, 22050), a2 = both (lim, 1.0f, 1000.0, 22050);
                double same = 0; for (size_t i = 0; i < a1.l.size(); ++i) same = std::max (same, (double) std::abs (a1.l[i] - a2.l[i]));
                const double dUnit = std::abs (lvl (both (other, 1.0f, 1000.0).l, 1000.0) - fullL), d16 = std::abs (lvl (both (hq, 1.0f, 1000.0).l, 1000.0) - fullL);
                check (worstDry < 1.0e-5 && dUnit > 0.003 && dUnit < 2.0 && same < 1.0e-6 && d16 < 0.05,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ slightly (" + juce::String (dUnit, 3) + " dB of reduction), the same unit is the same every time, 16x compresses as 4x does (" + juce::String (d16, 3) + " dB apart)");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageMu") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Mu","preset":"Bass Bus"})");
                const bool presetOk = pp.contains ("Bass Bus") && std::abs (vp->threshold->getCurrentValue() + 16.0f) < 0.01f && vp->mode->getCurrentValueAsString() == "Limit";
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Mu","params":[{"param":"Mode","text":"Compress"},{"param":"Recovery","text":"4 s"},{"param":"Threshold","text":"-20"},{"param":"Quality","text":"8x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->mode->getCurrentValueAsString() == "Compress" && vp->recovery->getCurrentValueAsString() == "4 s" && std::abs (vp->threshold->getCurrentValue() + 20.0f) < 0.01f && juce::roundToInt (vp->quality->getCurrentValue()) == 1
                       && Edition::packOf ("sqmVoltageMu") == Edition::Pack::color && Skins::artFor ("voltagemu").isValid(),
                       "Voltage Mu has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets Threshold and Limit, and the assistant sets Mode, Recovery, Threshold and Quality by name");
            }
        }
        // ---- 1.46 Voltage Brigade: the bucket-brigade delay as a plug-in (the line is checked against a circuit simulator and a brute-force simulation in tests/bbd_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Brigade"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Brigade"})");
            SqVoltageBrigadePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageBrigadePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Brigade") && juce::roundToInt (vp->quality->getCurrentValue()) == 0 && vp->stages->getCurrentValueAsString() == "4096" && vp->time->getCurrentValueAsString() == "120 ms", "Voltage Brigade is registered, inserts by name and starts at 4x on its first preset (4096 stages, 120 ms)");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int lat = SqVoltageBrigadePlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float time = 100, feedback = 0, tone = 4.5f, rate = 0.5f, depth = 0, width = 0, mix = 100, output = 0; int stages = 2, quality = 0, unit = 1234, sync = 0; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageBrigadePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageBrigadePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->time, st.time); setp (tw->sync, (float) st.sync); setp (tw->stages, (float) st.stages); setp (tw->feedback, st.feedback); setp (tw->tone, st.tone); setp (tw->rate, st.rate); setp (tw->depth, st.depth); setp (tw->width, st.width); setp (tw->mix, st.mix); setp (tw->output, st.output);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                auto burst = std::function<float (int)> ([=] (int i) { const double t = i / sr; return t < 0.03 ? (float) (0.25 * std::sin (twoPi * 500.0 * t) * (0.5 - 0.5 * std::cos (twoPi * t / 0.03))) : 0.0f; });
                auto both = [&] (const Set& st, float amp, double hz, int N = 66150) { return render (st, N, tone (amp, hz), tone (amp, hz)); };
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 22050, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peakIn = [&] (const std::vector<float>& y, double fromS, double toS) { float p = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) p = std::max (p, std::abs (y[(size_t) i])); return 20 * std::log10 ((double) p + 1e-15); };
                Set base;
                // the delay is stages / (2 x clock): two settings of Time differ by exactly their difference; Time beyond the chip's clock range is held at the range's end
                auto lagBetween = [&] (const std::vector<float>& a, const std::vector<float>& b, int expect) { double best = -1e300; int at = 0; auto corr = [&] (int lag) { double s = 0; for (int i = 0; i < 12000; ++i) s += (double) a[(size_t) i] * (double) b[(size_t) (i + lag)]; return s; }; for (int lag = expect - 60; lag <= expect + 60; ++lag) { const double c = corr (lag); if (c > best) { best = c; at = lag; } } const double c0 = corr (at - 1), c1 = corr (at), c2 = corr (at + 1); return (at + 0.5 * (c0 - c2) / (c0 - 2 * c1 + c2)) / sr * 1000.0; };
                Set t150 = base; t150.time = 150.0f; const auto b100 = render (base, 22016, burst, burst), b150 = render (t150, 22016, burst, burst);
                std::vector<float> dryB; for (int i = 0; i < 22016; ++i) dryB.push_back (burst (i));
                const double more = lagBetween (b100.l, b150.l, (int) std::lround (0.05 * sr)), whole = lagBetween (dryB, b100.l, (int) std::lround (0.1 * sr) + lat);
                Set tShort = base; tShort.time = 5.0f; Set tEdge = base; tEdge.time = 20.48f; const auto s1 = render (tShort, 8192, burst, burst), s2 = render (tEdge, 8192, burst, burst); double sameEdge = 0; for (size_t i = 0; i < s1.l.size(); ++i) sameEdge = std::max (sameEdge, (double) std::abs (s1.l[i] - s2.l[i]));
                const auto shown = SqVoltageBrigadePlugin::measure (2, 5.0f, 4.5f, 35.0f), shown100 = SqVoltageBrigadePlugin::measure (2, 100.0f, 4.5f, 35.0f);
                check (std::abs (more - 50.0) < 0.6 && whole > 100.0 + 1000.0 * lat / sr - 1.5 && whole < 100.0 + 1000.0 * lat / sr + 1.5 && sameEdge < 1.0e-6 && std::abs (shown.delayMs - 20.48) < 0.001 && std::abs (shown.clockKHz - 100.0) < 0.001 && std::abs (shown100.clockKHz - 20.48) < 0.001,
                       "Time 150 ms comes " + juce::String (more, 2) + " ms after Time 100 ms, and 100 ms arrives " + juce::String (whole - 1000.0 * lat / sr, 2) + " ms after the dry sound (the chip plus its filters; one unit's clock is trimmed within 1 %); asked for 5 ms, 4096 stages give " + juce::String (shown.delayMs, 2) + " ms at their fastest clock, and the face says so");
                // a slower clock is darker, and a tone above half the clock comes back folded
                Set slow = base; slow.time = 400.0f; Set quick = base; quick.time = 50.0f; const double inDb = 20 * std::log10 (0.1);
                const double dSlow = lvl (both (slow, 0.1f, 1000.0, 88200).l, 1000.0) - inDb, dQuick = lvl (both (quick, 0.1f, 1000.0, 88200).l, 1000.0) - inDb;
                const auto folded = both (slow, 0.1f, 3000.0, 88200); const double clockSlow = 4096.0 / 0.8 * SqVoltageBrigadePlugin::tolerance (1234, 0, 0, 0.01), foldAt = clockSlow - 3000.0, foldDb = lvl (folded.l, foldAt) - inDb;
                check (dSlow < dQuick - 0.3 && dSlow > -1.2 && dQuick > -0.2 && foldDb > -4.5 && foldDb < -1.5,
                       "a 1 kHz tone comes back " + juce::String (dQuick, 2) + " dB at 50 ms and " + juce::String (dSlow, 2) + " dB at 400 ms (a slower clock holds each sample longer); a 3 kHz tone into the 400 ms setting's " + juce::String (clockSlow / 1000.0, 2) + " kHz clock comes back folded to " + juce::String (juce::roundToInt (foldAt)) + " Hz at " + juce::String (foldDb, 1) + " dB");
                // feedback: each repeat is the last one times Feedback; past 100 % it howls and the buckets hold it
                Set fbS = base; fbS.feedback = 35.0f; const auto rep = render (fbS, 22016, burst, burst); const double r1 = peakIn (rep.l, 0.09, 0.15), r2 = peakIn (rep.l, 0.19, 0.25), r3 = peakIn (rep.l, 0.29, 0.35);
                Set howl = base; howl.feedback = 110.0f; howl.tone = 12.0f; const auto hw = render (howl, 176128, std::function<float (int)> ([=] (int i) { return i < 22050 ? (float) std::sin (twoPi * 300.0 * i / sr) : 0.0f; }), tone (0.0f, 1.0));
                const double howlPeak = peakIn (hw.l, 0.0, 4.0), howlLate = peakIn (hw.l, 3.5, 3.99);
                check (std::abs ((r2 - r1) + 9.12) < 0.6 && std::abs ((r3 - r2) + 9.12) < 0.8 && howlPeak < 2.5 && howlLate > -12.0,
                       "Feedback 35 %: the repeats fall " + juce::String (r1 - r2, 2) + " and " + juce::String (r2 - r3, 2) + " dB each (35 % is 9.12 dB); at 110 % it is still sounding at " + juce::String (howlLate, 1) + " dBFS three seconds after the input stopped and never passes " + juce::String (howlPeak, 1) + " dBFS - the buckets hold it");
                // the buckets bend a full-scale tone by the data sheet's 2.5 %
                const auto loud = both (base, 1.0f, 500.0), soft = both (base, 0.25f, 500.0); const double h3 = lvl (loud.l, 1500.0) - lvl (loud.l, 500.0), h3s = lvl (soft.l, 1500.0) - lvl (soft.l, 500.0);
                check (h3 > -35.0 && h3 < -29.5 && h3s < h3 - 20.0, "a full-scale tone comes back with its 3rd harmonic at " + juce::String (h3, 1) + " dB (the chip's data sheet: 2.5 % at that level, which is -32 dB); 12 dB quieter it is at " + juce::String (h3s, 1) + " dB");
                // wobbling the clock wobbles the pitch; Width sets the right side's wobble against the left's
                Set vib = base; vib.stages = 1; vib.time = 10.24f; vib.rate = 5.0f; vib.depth = 20.0f; const auto vb = both (vib, 0.2f, 1000.0, 88200);
                std::vector<double> zc; for (size_t i = 44100; i + 1 < vb.l.size(); ++i) if (vb.l[i] <= 0 && vb.l[i + 1] > 0) zc.push_back ((double) i + (double) vb.l[i] / ((double) vb.l[i] - (double) vb.l[i + 1]));
                double flo = 1e9, fhi = 0; for (size_t i = 8; i < zc.size(); ++i) { const double fr = 8.0 * sr / (zc[i] - zc[i - 8]); flo = std::min (flo, fr); fhi = std::max (fhi, fr); }
                Set wide = vib; wide.width = 100.0f; const auto wd = both (wide, 0.2f, 1000.0, 88200); double dNarrow = 0, dWide = 0; for (size_t i = 44100; i < vb.l.size(); ++i) { dNarrow = std::max (dNarrow, (double) std::abs (vb.l[i] - vb.r[i])); dWide = std::max (dWide, (double) std::abs (wd.l[i] - wd.r[i])); }
                check ((fhi - flo) / 2 > 13.0 && (fhi - flo) / 2 < 19.0 && dWide > 0.05 && dWide > 5.0 * dNarrow,
                       "Depth 20 % at 5 Hz through 1024 stages swings a 1 kHz tone " + juce::String ((fhi - flo) / 2, 1) + " Hz either way (the arithmetic says about 16); Width 100 % sets the two sides against each other (they differ by " + juce::String (dWide, 3) + ", against " + juce::String (dNarrow, 4) + " at Width 0)");
                // Sync: the delay is a note length at the session's tempo, exactly as if Time had been set to it
                {
                    const double bpm = session->edit->tempoSequence.getBpmAt (te::TimePosition()), eighth = 30000.0 / bpm; Set synced = base; synced.stages = 3; synced.sync = 4; synced.time = 30.0f; Set byHand = base; byHand.stages = 3; byHand.time = (float) eighth;
                    const auto ys = render (synced, 22016, burst, burst), yh = render (byHand, 22016, burst, burst); double worstSync = 0, heard = 0; for (size_t i = 0; i < ys.l.size(); ++i) { worstSync = std::max (worstSync, (double) std::abs (ys.l[i] - yh.l[i])); heard = std::max (heard, (double) std::abs (ys.l[i])); }
                    check (worstSync < 1.0e-6 && heard > 0.05 && SqVoltageBrigadePlugin::syncNames()[4] == "1/8" && std::abs (SqVoltageBrigadePlugin::askedMs (30.0f, 5, 120.0) - 375.0) < 1.0e-9 && std::abs (SqVoltageBrigadePlugin::askedMs (30.0f, 0, 120.0) - 30.0) < 1.0e-9,
                           "Sync 1/8 at the session's " + juce::String (bpm, 1) + " BPM gives the same sound as Time set by hand to " + juce::String (eighth, 1) + " ms (largest difference " + juce::String (worstSync, 8) + "); a dotted eighth at 120 BPM asks for 375 ms; Sync off leaves the knob in charge");
                }
                Set dryS = base; dryS.mix = 0.0f; dryS.feedback = 60.0f; const auto dry = both (dryS, 0.5f, 1000.0, 33075); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.5 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = base; other.unit = 99999; Set hq = base; hq.quality = 2; Set up = base; up.output = 6.0f; const auto a1 = both (base, 0.5f, 1000.0, 22050), a2 = both (base, 0.5f, 1000.0, 22050);
                double same = 0; for (size_t i = 0; i < a1.l.size(); ++i) same = std::max (same, (double) std::abs (a1.l[i] - a2.l[i]));
                const double ref1k = lvl (both (base, 0.5f, 1000.0).l, 1000.0), d16 = std::abs (lvl (both (hq, 0.5f, 1000.0).l, 1000.0) - ref1k), up6 = lvl (both (up, 0.5f, 1000.0).l, 1000.0) - ref1k; const auto ou = both (other, 0.5f, 1000.0, 22050);
                double differ = 0; for (size_t i = 8000; i < a1.l.size(); ++i) differ = std::max (differ, (double) std::abs (a1.l[i] - ou.l[i]));
                check (worstDry < 1.0e-5 && differ > 0.001 && same < 1.0e-6 && d16 < 0.05 && std::abs (up6 - 6.0) < 0.02,
                       "Mix at 0 % is the input delayed by 96 samples; two units differ (their clocks and capacitors: " + juce::String (differ, 3) + "), the same unit is the same every time, 16x gives the level 4x does (" + juce::String (d16, 3) + " dB apart), Output 6 dB up is " + juce::String (up6, 2) + " dB up");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageBrigade") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Brigade","preset":"Wide Chorus"})");
                const bool presetOk = pp.contains ("Wide Chorus") && vp->stages->getCurrentValueAsString() == "256" && std::abs (vp->width->getCurrentValue() - 100.0f) < 0.01f;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Brigade","params":[{"param":"Stages","text":"8192"},{"param":"Time","text":"400"},{"param":"Feedback","text":"50"},{"param":"Sync","text":"1/8 D"},{"param":"Quality","text":"8x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->stages->getCurrentValueAsString() == "8192" && std::abs (vp->time->getCurrentValue() - 400.0f) < 0.5f && std::abs (vp->feedback->getCurrentValue() - 50.0f) < 0.01f && juce::roundToInt (vp->quality->getCurrentValue()) == 1 && vp->sync->getCurrentValueAsString() == "1/8 D"
                       && Edition::packOf ("sqmVoltageBrigade") == Edition::Pack::color && Skins::artFor ("voltagebrigade").isValid(),
                       "Voltage Brigade has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets the 256-stage chip and full Width, and the assistant sets Stages, Time, Feedback, Sync and Quality by name");
            }
        }
        // ---- 1.47 Voltage Spring: the spring reverb as a plug-in (the spring's modes are checked against the equations stepped directly in time in tests/spring_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Spring"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Spring"})");
            SqVoltageSpringPlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageSpringPlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Spring") && vp->tank->getCurrentValueAsString() == "Long trio" && vp->decay->getCurrentValueAsString() == "2.5 s" && vp->getLatencySeconds() == 0.0, "Voltage Spring is registered, inserts by name on its first preset (the long trio, 2.5 s) and adds no delay");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi;
                struct Out { std::vector<float> l, r; };
                struct Set { float length = 100, decay = 2.5f, damping = 4, lowCut = 60, width = 0, mix = 100, output = 0; int tank = 0, unit = 1234; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageSpringPlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageSpringPlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->tank, (float) st.tank); setp (tw->length, st.length); setp (tw->decay, st.decay); setp (tw->damping, st.damping); setp (tw->lowCut, st.lowCut); setp (tw->width, st.width); setp (tw->mix, st.mix); setp (tw->output, st.output);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto same = [&] (const Set& st, int N, const std::function<float (int)>& f) { return render (st, N, f, f); };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                std::vector<float> noise ((size_t) 300000); { juce::Random rng (4242); for (auto& v : noise) v = 0.1f * 1.7320508f * (rng.nextFloat() * 2.0f - 1.0f); }      // white, rms 0.1
                auto white = std::function<float (int)> ([&noise] (int i) { return i < (int) noise.size() ? noise[(size_t) i] : 0.0f; });
                auto rms = [&] (const std::vector<float>& y, double fromS, double toS) { double s = 0; int n = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) { s += (double) y[(size_t) i] * y[(size_t) i]; ++n; } return 10 * std::log10 (s / juce::jmax (1, n) + 1e-30); };
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 22050, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                Set base;
                Set dryS = base; dryS.mix = 0.0f; const auto dry = same (dryS, 22016, white); double worstDry = 0; for (int i = 0; i < 22016; ++i) worstDry = std::max (worstDry, (double) std::abs (dry.l[(size_t) i] - noise[(size_t) i]));
                // every echo is a chirp: a low burst reaches the far end after half the echo spacing, a high one later
                const auto spring = SqVoltageSpringPlugin::springFor (1234, 0, 0, 100.0f);
                auto arrival = [&] (double f) { const double t0 = 0.005, wdt = 0.0012; const auto y = same (base, 4096, std::function<float (int)> ([=] (int i) { const double t = i / sr, x = (t - t0) / wdt; return (float) (std::exp (-0.5 * x * x) * std::sin (twoPi * f * t)); }));
                    double er = 0, ei = 0, best = 0, at = 0; const double a = std::exp (-1.0 / (0.0005 * sr)); for (int i = 0; i < (int) y.l.size(); ++i) { const double t = i / sr; er = a * er + (1 - a) * y.l[(size_t) i] * std::cos (twoPi * f * t); ei = a * ei + (1 - a) * y.l[(size_t) i] * std::sin (twoPi * f * t); const double env = er * er + ei * ei, arr = t - t0 - 0.0005; if (arr > 0.016 && arr < 0.045 && env > best) { best = env; at = arr; } } return at * 1000.0; };
                const double aLow = arrival (500.0), aHigh = arrival (3500.0), half = 500.0 * spring.echoSeconds();
                check (worstDry < 1.0e-7 && std::abs (aLow - half) < 0.06 * half && aHigh > aLow + 5.0 && aHigh < aLow + 11.0,
                       "Mix at 0 % is the input untouched; through one spring a 500 Hz burst reaches the far end after " + juce::String (aLow, 1) + " ms (half the " + juce::String (2.0 * half, 1) + " ms echo spacing its turns, coil and wire give) and a 3.5 kHz burst " + juce::String (aHigh - aLow, 1) + " ms later still - every echo is a chirp");
                // decay: after a burst of low noise, the tail falls at the rate asked for
                std::vector<float> lowNoise ((size_t) 8820); { float a1 = 0, a2 = 0; for (int i = 0; i < 8820; ++i) { a1 += 0.15f * (noise[(size_t) i] * 6.0f - a1); a2 += 0.15f * (a1 - a2); lowNoise[(size_t) i] = a2; } }
                auto burstIn = std::function<float (int)> ([&lowNoise] (int i) { return i < (int) lowNoise.size() ? lowNoise[(size_t) i] : 0.0f; });
                Set d1 = base; d1.decay = 1.0f; d1.damping = 12.0f; Set d4 = d1; d4.decay = 4.0f; const auto y1 = same (d1, 88064, burstIn), y4 = same (d4, 88064, burstIn);
                const double fall1 = rms (y1.l, 0.5, 0.7) - rms (y1.l, 1.5, 1.7), fall4 = rms (y4.l, 0.5, 0.7) - rms (y4.l, 1.5, 1.7);
                check (fall1 > 54.0 && fall1 < 76.0 && fall4 > 13.5 && fall4 < 19.5, "Decay: the tail of a burst falls " + juce::String (fall1, 1) + " dB in a second at 1 s and " + juce::String (fall4, 1) + " dB at 4 s (60 and 15 for the lowest modes; higher ones die a little faster)");
                // level: steady noise in comes out at about its own level whatever the decay (the plug-in sets its level from the modes' sum)
                Set dA = base; dA.decay = 1.0f; Set dB = base; dB.decay = 4.0f; const double lA = rms (same (dA, 264600, white).l, 4.0, 5.9), lB = rms (same (dB, 264600, white).l, 4.0, 5.9);
                check (std::abs (lA + 20.0) < 1.5 && std::abs (lB + 20.0) < 2.0, "steady noise at -20 dB comes out at " + juce::String (lA, 1) + " dB with a 1 s decay and " + juce::String (lB, 1) + " dB with 4 s: the level is set from the springs' own modes, so a longer decay is longer, not louder");
                Set cutLo = base; cutLo.lowCut = 20.0f; Set cutHi = base; cutHi.lowCut = 300.0f; const double c20 = lvl (same (cutLo, 132096, tone (0.5f, 70.0)).l, 70.0), c300 = lvl (same (cutHi, 132096, tone (0.5f, 70.0)).l, 70.0);
                Set trio = base; trio.tank = 2; Set wideS = trio; wideS.width = 100.0f; const auto n0 = same (trio, 44032, white), nw = same (wideS, 44032, white); double dMid = 0, dWide = 0; for (size_t i = 0; i < n0.l.size(); ++i) { dMid = std::max (dMid, (double) std::abs (n0.l[i] - n0.r[i])); dWide = std::max (dWide, (double) std::abs (nw.l[i] - nw.r[i])); }
                check (c300 < c20 - 10.0 && dMid < 1.0e-6 && dWide > 0.02, "Low Cut at 300 Hz takes a 70 Hz tone down " + juce::String (c20 - c300, 1) + " dB (its modes are left out); Width 0 puts all three springs in the middle (the two sides are the same), Width 100 spreads them (they differ by " + juce::String (dWide, 2) + ")");
                // it is a sum of resonators: nothing new is made, and twice the input is twice the output
                Set lin = base; lin.decay = 0.8f; const auto t1 = same (lin, 132096, tone (0.25f, 1000.0)), t2 = same (lin, 132096, tone (0.5f, 1000.0)); const double h2 = lvl (t1.l, 2000.0) - lvl (t1.l, 1000.0), h3 = lvl (t1.l, 3000.0) - lvl (t1.l, 1000.0), dbl = lvl (t2.l, 1000.0) - lvl (t1.l, 1000.0);
                Set other = base; other.unit = 99999; const auto u1 = same (base, 22016, white), u2 = same (base, 22016, white), u3 = same (other, 22016, white); double sameU = 0, differ = 0; for (size_t i = 0; i < u1.l.size(); ++i) { sameU = std::max (sameU, (double) std::abs (u1.l[i] - u2.l[i])); differ = std::max (differ, (double) std::abs (u1.l[i] - u3.l[i])); }
                check (h2 < -100.0 && h3 < -100.0 && std::abs (dbl - 6.0206) < 0.001 && sameU < 1.0e-7 && differ > 0.01,
                       "a tone in is the same tone out and nothing else (2nd and 3rd harmonics " + juce::String (juce::roundToInt (h2)) + " and " + juce::String (juce::roundToInt (h3)) + " dB); twice the input is " + juce::String (dbl, 3) + " dB more out; the same unit is the same every time and two units are wound differently (" + juce::String (differ, 2) + ")");
                {
                    const auto shown = SqVoltageSpringPlugin::measure (1234, 2, 100.0f, 2.5f, 4.0f, 60.0f);
                    check (shown.springs == 3 && shown.modes > 2500 && shown.modes < 3300 && std::abs (shown.echoMs[0] - 40.5) < 1.5 && std::abs (shown.transitionHz[0] - 5038.0) < 150.0 && shown.echoMs[2] > shown.echoMs[0] + 6.0 && std::abs (shown.decayLow - 2.5 / (1.0 + 1.0 / 64.0)) < 0.01 && std::abs (shown.decayHigh - 2.5 / 2.5625) < 0.01 && shown.curveMs[0][23] > 2.0f * shown.curveMs[0][0],
                           "the face shows the springs' own numbers: three springs, " + juce::String (shown.modes) + " modes sounding, echoes every " + juce::String (shown.echoMs[0], 1) + " / " + juce::String (shown.echoMs[1], 1) + " / " + juce::String (shown.echoMs[2], 1) + " ms, chirps topping out at " + juce::String (juce::roundToInt (shown.transitionHz[0])) + " Hz, 60 dB down in " + juce::String (shown.decayLow, 2) + " s at 500 Hz and " + juce::String (shown.decayHigh, 2) + " s at 5 kHz");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageSpring") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Spring","preset":"Guitar Amp"})");
                const bool presetOk = pp.contains ("Guitar Amp") && vp->tank->getCurrentValueAsString() == "Short pair" && std::abs (vp->decay->getCurrentValue() - 2.0f) < 0.01f;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Spring","params":[{"param":"Tank","text":"Single"},{"param":"Decay","text":"3"},{"param":"Low Cut","text":"200"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->tank->getCurrentValueAsString() == "Single" && std::abs (vp->decay->getCurrentValue() - 3.0f) < 0.01f && std::abs (vp->lowCut->getCurrentValue() - 200.0f) < 0.5f
                       && Edition::packOf ("sqmVoltageSpring") == Edition::Pack::color && Skins::artFor ("voltagespring").isValid(),
                       "Voltage Spring has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset picks the short pair, and the assistant sets Tank, Decay and Low Cut by name");
            }
        }
        // ---- 1.48 Voltage Plate: the plate reverb as a plug-in (the plate's modes are checked against the equation stepped directly in time in tests/plate_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Plate"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Plate"})");
            SqVoltagePlatePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltagePlatePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Plate") && vp->plate->getCurrentValueAsString() == "Classic" && vp->decay->getCurrentValueAsString() == "2.5 s" && vp->getLatencySeconds() == 0.0, "Voltage Plate is registered, inserts by name on its first preset (the classic plate, 2.5 s) and adds no delay");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi;
                struct Out { std::vector<float> l, r; };
                struct Set { float decay = 1.0f, damping = 12.0f, lowCut = 80, width = 100, mix = 100, output = 0; int plate = 0, unit = 1234; };
                auto keyOf = [&] (const Set& st) { SqVoltagePlatePlugin::Key k; k.plateIx = st.plate; k.unitSeed = st.unit; k.decay = st.decay; k.damping = st.damping; k.lowCut = st.lowCut; k.rate = sr; return k; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltagePlatePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltagePlatePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->plate, (float) st.plate); setp (tw->decay, st.decay); setp (tw->damping, st.damping); setp (tw->lowCut, st.lowCut); setp (tw->width, st.width); setp (tw->mix, st.mix); setp (tw->output, st.output);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto same = [&] (const Set& st, int N, const std::function<float (int)>& f) { return render (st, N, f, f); };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                std::vector<float> noise ((size_t) 300000); { juce::Random rng (777); for (auto& v : noise) v = 0.1f * 1.7320508f * (rng.nextFloat() * 2.0f - 1.0f); }
                auto white = std::function<float (int)> ([&noise] (int i) { return i < (int) noise.size() ? noise[(size_t) i] : 0.0f; });
                auto rms = [&] (const std::vector<float>& y, double fromS, double toS) { double s = 0; int n = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) { s += (double) y[(size_t) i] * y[(size_t) i]; ++n; } return 10 * std::log10 (s / juce::jmax (1, n) + 1e-30); };
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 22050, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                Set base;
                Set dryS = base; dryS.mix = 0.0f; const auto dry = same (dryS, 22016, white); double worstDry = 0; for (int i = 0; i < 22016; ++i) worstDry = std::max (worstDry, (double) std::abs (dry.l[(size_t) i] - noise[(size_t) i]));
                // a single tap in comes out as the plate's cast answer, from the very first sample: the convolution adds no delay and changes nothing
                int modes = 0; const auto tap = SqVoltagePlatePlugin::cast (keyOf (base), &modes); const auto hit = same (base, 44032, std::function<float (int)> ([] (int i) { return i == 0 ? 1.0f : 0.0f; }));
                double worstTap = 0, tapPeak = 0; for (int i = 0; i < 44032 && i < tap.getNumSamples(); ++i) { worstTap = std::max (worstTap, (double) std::abs (hit.l[(size_t) i] - tap.getSample (0, i))); worstTap = std::max (worstTap, (double) std::abs (hit.r[(size_t) i] - tap.getSample (1, i))); tapPeak = std::max (tapPeak, (double) std::abs (tap.getSample (0, i))); }
                check (worstDry < 1.0e-7 && tapPeak > 0.0 && worstTap < 1.0e-3 * tapPeak && std::abs (tap.getSample (0, 0)) > 1.0e-4 * tapPeak && modes > 6000 && modes < 6700,
                       "Mix at 0 % is the input untouched; a single tap comes out as the small plate's cast answer from the very first sample - the sum of its " + juce::String (modes) + " modes - to " + juce::String (juce::roundToInt (20 * std::log10 (worstTap / tapPeak + 1e-12))) + " dB of its peak over a second");
                std::vector<float> lowNoise ((size_t) 8820); { float a1 = 0, a2 = 0; for (int i = 0; i < 8820; ++i) { a1 += 0.15f * (noise[(size_t) i] * 6.0f - a1); a2 += 0.15f * (a1 - a2); lowNoise[(size_t) i] = a2; } }
                auto burstIn = std::function<float (int)> ([&lowNoise] (int i) { return i < (int) lowNoise.size() ? lowNoise[(size_t) i] : 0.0f; });
                Set d4 = base; d4.decay = 4.0f; const auto y1 = same (base, 88064, burstIn), y4 = same (d4, 88064, burstIn);
                const double fall1 = rms (y1.l, 0.5, 0.7) - rms (y1.l, 1.5, 1.7), fall4 = rms (y4.l, 0.5, 0.7) - rms (y4.l, 1.5, 1.7);
                check (fall1 > 54.0 && fall1 < 74.0 && fall4 > 13.5 && fall4 < 19.5, "Decay: the tail of a burst falls " + juce::String (fall1, 1) + " dB in a second at 1 s and " + juce::String (fall4, 1) + " dB at 4 s (60 and 15 for the lowest modes; higher ones die a little faster)");
                Set big = base; big.plate = 1; big.decay = 2.0f; big.damping = 5.0f; const double lA = rms (same (base, 220160, white).l, 3.0, 4.9), lB = rms (same (d4, 264192, white).l, 4.0, 5.9), lC = rms (same (big, 264192, white).l, 4.0, 5.9);
                check (std::abs (lA + 20.0) < 1.5 && std::abs (lB + 20.0) < 2.0 && std::abs (lC + 20.0) < 1.5, "steady noise at -20 dB comes out at " + juce::String (lA, 1) + " dB with a 1 s decay, " + juce::String (lB, 1) + " dB with 4 s, and " + juce::String (lC, 1) + " dB through the classic plate: the level is set from the modes' own sum");
                Set cutHi = base; cutHi.lowCut = 400.0f; const double c80 = lvl (same (base, 132096, tone (0.5f, 150.0)).l, 150.0), c400 = lvl (same (cutHi, 132096, tone (0.5f, 150.0)).l, 150.0);
                Set mono = base; mono.width = 0.0f; const auto nw = same (base, 44032, white), n0 = same (mono, 44032, white); double dWide = 0, dMono = 0; for (size_t i = 0; i < nw.l.size(); ++i) { dWide = std::max (dWide, (double) std::abs (nw.l[i] - nw.r[i])); dMono = std::max (dMono, (double) std::abs (n0.l[i] - n0.r[i])); }
                check (c400 < c80 - 10.0 && dMono < 1.0e-6 && dWide > 0.02, "Low Cut at 400 Hz takes a 150 Hz tone down " + juce::String (c80 - c400, 1) + " dB (its modes are left out of the cast); Width 0 makes the two pickups one (the sides are the same), Width 100 keeps them apart (they differ by " + juce::String (dWide, 2) + ")");
                const auto t1 = same (base, 132096, tone (0.25f, 1000.0)), t2 = same (base, 132096, tone (0.5f, 1000.0)); const double h2 = lvl (t1.l, 2000.0) - lvl (t1.l, 1000.0), h3 = lvl (t1.l, 3000.0) - lvl (t1.l, 1000.0), dbl = lvl (t2.l, 1000.0) - lvl (t1.l, 1000.0);
                Set other = base; other.unit = 99999; const auto u1 = same (base, 22016, white), u2 = same (base, 22016, white), u3 = same (other, 22016, white); double sameU = 0, differ = 0; for (size_t i = 0; i < u1.l.size(); ++i) { sameU = std::max (sameU, (double) std::abs (u1.l[i] - u2.l[i])); differ = std::max (differ, (double) std::abs (u1.l[i] - u3.l[i])); }
                check (h2 < -90.0 && h3 < -90.0 && std::abs (dbl - 6.0206) < 0.005 && sameU < 1.0e-6 && differ > 0.01,
                       "a tone in is the same tone out and nothing else (2nd and 3rd harmonics " + juce::String (juce::roundToInt (h2)) + " and " + juce::String (juce::roundToInt (h3)) + " dB); twice the input is " + juce::String (dbl, 3) + " dB more out; the same unit is the same every time and two units' plates differ (" + juce::String (differ, 2) + ")");
                {
                    SqVoltagePlatePlugin::Key k; k.plateIx = 1; k.unitSeed = 1234; k.decay = 2.5f; k.damping = 5.0f; k.lowCut = 80.0f; k.rate = sr; const auto shown = SqVoltagePlatePlugin::measure (k); k.plateIx = 0; const auto shownSmall = SqVoltagePlatePlugin::measure (k);
                    check (shown.modes > 25000 && shown.modes < 27000 && std::abs (shown.modesPerHz - 1.31) < 0.04 && shownSmall.modes > 6200 && shownSmall.modes < 6700 && std::abs (shown.decayLow - 2.5 / (1.0 + 0.01)) < 0.01 && std::abs (shown.decayHigh - 1.25) < 0.01 && shown.lengthM == 2.0 && shown.curveSeconds[23] < 0.2f * shown.curveSeconds[0],
                           "the face shows the plate's own numbers: 2.0 x 1.0 m, " + juce::String (shown.modes) + " modes sounding (" + juce::String (shown.modesPerHz, 2) + " in every hertz; the small plate has " + juce::String (shownSmall.modes) + "), 60 dB down in " + juce::String (shown.decayLow, 2) + " s at 500 Hz and " + juce::String (shown.decayHigh, 2) + " s at 5 kHz");
                }
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltagePlate") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Plate","preset":"Short Bright"})");
                const bool presetOk = pp.contains ("Short Bright") && vp->plate->getCurrentValueAsString() == "Small" && std::abs (vp->decay->getCurrentValue() - 1.0f) < 0.01f;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Plate","params":[{"param":"Plate","text":"Classic"},{"param":"Decay","text":"3"},{"param":"Damping","text":"4"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->plate->getCurrentValueAsString() == "Classic" && std::abs (vp->decay->getCurrentValue() - 3.0f) < 0.01f && std::abs (vp->damping->getCurrentValue() - 4.0f) < 0.01f
                       && Edition::packOf ("sqmVoltagePlate") == Edition::Pack::color && Skins::artFor ("voltageplate").isValid(),
                       "Voltage Plate has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate art, a preset picks the small plate, and the assistant sets Plate, Decay and Damping by name");
            }
        }
        // ---- 1.49 Voltage Bridge: the diode-bridge compressor as a plug-in (the ring and the loop are checked against a circuit simulator in tests/bridge_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Bridge"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Bridge"})");
            SqVoltageBridgePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageBridgePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Bridge") && juce::roundToInt (vp->quality->getCurrentValue()) == 0 && vp->ratio->getCurrentValueAsString() == "4:1" && vp->attack->getCurrentValueAsString() == "Slow" && vp->recovery->getCurrentValueAsString() == "100 ms",
                   "Voltage Bridge is registered, inserts by name on its first preset (4:1, Slow attack, 100 ms recovery) at 4x");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int lat = SqVoltageBridgePlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float threshold = -14, gain = 0, mix = 100; int ratio = 3, attack = 0, recovery = 1, quality = 0, unit = 1234; };
                auto render = [&] (const Set& st, int N, const std::function<float (int)>& left, const std::function<float (int)>& right)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageBridgePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageBridgePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->threshold, st.threshold); setp (tw->ratio, (float) st.ratio); setp (tw->attack, (float) st.attack); setp (tw->recovery, (float) st.recovery); setp (tw->gain, st.gain); setp (tw->mix, st.mix);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, left (q + i)); buf.setSample (1, i, right (q + i)); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto tone = [&] (float amp, double hz) { return std::function<float (int)> ([=] (int i) { return amp * (float) std::sin (twoPi * hz * i / sr); }); };
                auto both = [&] (const Set& st, float amp, double hz, int N = 88200) { return render (st, N, tone (amp, hz), tone (amp, hz)); };
                auto lvl = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int M = 8820, from = (int) y.size() - M; if (from < 0) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * (from + i) / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peakIn = [&] (const std::vector<float>& y, double fromS, double toS) { float p = 0; for (int i = (int) (fromS * sr); i < (int) (toS * sr) && i < (int) y.size(); ++i) p = std::max (p, std::abs (y[(size_t) i])); return 20 * std::log10 ((double) p + 1e-15); };
                Set base;
                const auto quiet = both (base, 0.05f, 1000.0, 22050); const double quietDb = 20 * std::log10 (0.05), q0 = lvl (quiet.l, 1000.0) - quietDb;
                double corr = 0; for (int i = 11025; i < 11025 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * 0.05 * std::sin (twoPi * 1000.0 * (i - lat) / sr);
                check (std::abs (q0) < 0.05 && corr > 0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "Voltage Bridge leaves a tone under its threshold alone (" + juce::String (q0, 3) + " dB), the right way up and 96 samples late, as it reports");
                // the ratios, as the published test does it: 10 dB more input between 20 and 30 dB over the threshold
                Set deep = base; deep.threshold = -30.0f; juce::String ratioLine; bool ratiosOk = true; const double want[5] = { 6.5, 5.0, 3.5, 2.5, 1.2 }, tol[5] = { 1.0, 1.0, 1.0, 0.6, 0.5 };
                for (int ix = 0; ix < 5; ++ix) { Set s2 = deep; s2.ratio = ix; const double d = lvl (both (s2, 1.0f, 1000.0).l, 1000.0) - lvl (both (s2, 0.3162f, 1000.0).l, 1000.0); ratiosOk = ratiosOk && std::abs (d - want[ix]) <= tol[ix]; ratioLine << " " << juce::String (d, 2); }
                check (ratiosOk, "10 dB more input, 20 to 30 dB over the threshold, comes out as" + ratioLine + " dB more at 1.5, 2, 3, 4 and 6 to 1 (published for such compressors: 6.5, 5.0, 3.5 and 2.5 dB at the first four)");
                const double full = lvl (both (base, 1.0f, 1000.0).l, 1000.0);
                {
                    const auto shown = SqVoltageBridgePlugin::measure (1234, -14.0f, 3, false, 1); const auto shownDeep = SqVoltageBridgePlugin::measure (1234, -30.0f, 3, false, 1);
                    check (std::abs (shown.reductionAtFull - full) < 0.5 && std::abs (10.0 / shownDeep.ratio - 2.1) < 0.5 && shown.third < -50.0 && shown.third > -95.0 && shown.microampsAtFull > 1.0 && shown.microampsAtFull < 5000.0 && std::abs (shown.out[0] - shown.in[0]) < 0.05 && full < -4.0,
                           "the face works the same numbers out from the diodes without running the audio: a full-scale tone at " + juce::String (shown.reductionAtFull, 1) + " dB (audio " + juce::String (full, 1) + "), ratio " + juce::String (shownDeep.ratio, 1) + " : 1, ring current " + juce::String (shown.microampsAtFull, 0) + " microamps, 3rd harmonic " + juce::String (shown.third, 0) + " dB");
                }
                // recovery: after a loud passage stops, a quiet tone is back quickly at 100 ms and held down at 1.5 s; the automatic setting holds on after a long passage
                auto drop = std::function<float (int)> ([=] (int i) { return (i < (int) (1.5 * sr) ? 1.0f : 0.02f) * (float) std::sin (twoPi * 1000.0 * i / sr); });
                const double tinyDb = 20 * std::log10 (0.02);
                Set fastR = base; fastR.threshold = -24.0f; fastR.recovery = 0; Set slowR = fastR; slowR.recovery = 3; Set autoR = fastR; autoR.recovery = 4;
                const double rFast = peakIn (render (fastR, 88200, drop, drop).l, 1.9, 1.95) - tinyDb, rSlow = peakIn (render (slowR, 88200, drop, drop).l, 1.9, 1.95) - tinyDb, rAuto = peakIn (render (autoR, 88200, drop, drop).l, 1.9, 1.95) - tinyDb;
                Set slowA = base; slowA.attack = 1; const double bassFast = lvl (both (base, 1.0f, 40.0).l, 40.0), bassSlow = lvl (both (slowA, 1.0f, 40.0).l, 40.0), midSlow = lvl (both (slowA, 1.0f, 1000.0).l, 1000.0);
                const double pFirst = peakIn (both (base, 1.0f, 1000.0, 4410).l, 0.0022, 0.0032), pLater = peakIn (both (base, 1.0f, 1000.0, 4410).l, 0.03, 0.05);
                check (rFast > -1.0 && rSlow < rFast - 4.0 && rAuto < rFast - 2.0 && bassSlow > bassFast + 1.0 && std::abs (midSlow - full) < 0.5 && pFirst > pLater + 3.0,
                       "Recovery: 0.4 s after a loud passage a quiet tone is " + juce::String (rFast, 1) + " dB down at 100 ms, " + juce::String (rSlow, 1) + " at 1.5 s and " + juce::String (rAuto, 1) + " on Auto 1; a sudden full-scale tone starts at " + juce::String (pFirst, 1) + " dBFS and settles to " + juce::String (pLater, 1)
                       + " within 30 ms; on Slow a 40 Hz tone comes out " + juce::String (bassSlow - bassFast, 1) + " dB louder than on Fast (the sidechain hears less bass) and a 1 kHz tone the same");
                Set loud = base; loud.gain = 6.0f; const double up6 = lvl (both (loud, 1.0f, 1000.0).l, 1000.0) - full;
                const auto link = render (base, 88200, tone (1.0f, 1000.0), tone (0.01f, 700.0)); const double gl = lvl (link.l, 1000.0), gr = lvl (link.r, 700.0) + 40.0;
                const auto solo = render (base, 88200, tone (0.0f, 1000.0), tone (1.0f, 700.0)); const double gs = lvl (solo.r, 700.0);
                check (std::abs (up6 - 6.0) < 0.02 && gl < -4.0 && std::abs (gl - gr) < 0.5 && gs < -4.0, "Gain 6 dB up is " + juce::String (up6, 2) + " dB up; both channels share one sidechain: a full-scale left (" + juce::String (gl, 2) + " dB) takes a quiet right down with it (" + juce::String (gr, 2) + " dB), and a loud right on its own is compressed too (" + juce::String (gs, 2) + " dB)");
                Set dryS = base; dryS.mix = 0.0f; const auto dry = both (dryS, 0.5f, 1000.0, 33075); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.5 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = base; other.unit = 99999; Set hq = base; hq.quality = 2; const auto a1 = both (base, 1.0f, 1000.0, 22050), a2 = both (base, 1.0f, 1000.0, 22050), a3 = both (other, 1.0f, 1000.0, 22050);
                double same = 0, differ = 0; for (size_t i = 0; i < a1.l.size(); ++i) { same = std::max (same, (double) std::abs (a1.l[i] - a2.l[i])); differ = std::max (differ, (double) std::abs (a1.l[i] - a3.l[i])); }
                const double d16 = std::abs (lvl (both (hq, 1.0f, 1000.0).l, 1000.0) - full);
                check (worstDry < 1.0e-5 && differ > 1.0e-6 && differ < 0.1 && same < 1.0e-6 && d16 < 0.05,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ slightly (by at most " + juce::String (differ, 5) + " of full scale), the same unit is the same every time, 16x compresses as 4x does (" + juce::String (d16, 3) + " dB apart)");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageBridge") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Bridge","preset":"Mix Bus Glue"})");
                const bool presetOk = pp.contains ("Mix Bus Glue") && std::abs (vp->threshold->getCurrentValue() + 10.0f) < 0.01f && vp->recovery->getCurrentValueAsString() == "Auto 1" && vp->ratio->getCurrentValueAsString() == "2:1";
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Bridge","params":[{"param":"Ratio","text":"6:1"},{"param":"Recovery","text":"1.5 s"},{"param":"Attack","text":"Fast"},{"param":"Threshold","text":"-20"},{"param":"Quality","text":"8x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->ratio->getCurrentValueAsString() == "6:1" && vp->recovery->getCurrentValueAsString() == "1.5 s" && vp->attack->getCurrentValueAsString() == "Fast" && std::abs (vp->threshold->getCurrentValue() + 20.0f) < 0.01f && juce::roundToInt (vp->quality->getCurrentValue()) == 1
                       && Edition::packOf ("sqmVoltageBridge") == Edition::Pack::color && Skins::artFor ("voltagebridge").isValid(),
                       "Voltage Bridge has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets Threshold, Ratio and automatic recovery, and the assistant sets Ratio, Recovery, Attack, Threshold and Quality by name");
            }
        }

        // ---- 1.50 Voltage Overdrive: the soft clipper as a plug-in (the circuit is checked against a circuit simulator in tests/drive_test.cpp)
        {
            call ("new_session", R"({"name":"Selftest Voltage Overdrive"})");
            const auto ins = call ("insert_plugin", R"({"track":"Audio 1","plugin":"Voltage Overdrive"})");
            SqVoltageOverdrivePlugin* vp = nullptr;
            if (auto tt = session->findTrack ("Audio 1")) for (auto pl : tt->pluginList) if (auto c = dynamic_cast<SqVoltageOverdrivePlugin*> (pl)) vp = c;
            check (vp != nullptr && ins.contains ("Voltage Overdrive") && juce::roundToInt (vp->quality->getCurrentValue()) == 1 && juce::roundToInt (vp->drive->getCurrentValue()) == 20, "Voltage Overdrive is registered, inserts by name on its first preset (Drive 20 %) at 8x");
            if (vp != nullptr)
            {
                const double sr = 44100.0, twoPi = juce::MathConstants<double>::twoPi; const int N = 44100, lat = SqVoltageOverdrivePlugin::delaySamples;
                struct Out { std::vector<float> l, r; };
                struct Set { float drive = 50, tone = 100, mix = 100; int diodes = 0, sym = 0, quality = 1, unit = 1234; };
                auto render = [&] (const Set& st, float amp, double hz)
                {
                    Out o; auto twin = session->edit->getPluginCache().createNewPlugin (SqVoltageOverdrivePlugin::xmlTypeName, {});
                    if (auto* tw = dynamic_cast<SqVoltageOverdrivePlugin*> (twin.get()))
                    {
                        tw->unit = st.unit; auto setp = [] (te::AutomatableParameter::Ptr& p, float v) { p->setParameter (v, juce::dontSendNotification); };
                        setp (tw->quality, (float) st.quality); setp (tw->drive, st.drive); setp (tw->tone, st.tone); setp (tw->mix, st.mix); setp (tw->diodes, (float) st.diodes); setp (tw->symmetry, (float) st.sym); setp (tw->output, 0.0f);
                        tw->baseClassInitialise ({ te::TimePosition(), sr, 512 });
                        juce::AudioBuffer<float> buf (2, 512); te::MidiMessageArray midi;
                        for (int q = 0; q + 512 <= N; q += 512)
                        {
                            for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (twoPi * hz * (q + i) / sr); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                            te::PluginRenderContext fc (&buf, 0, 512, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (q / sr), te::TimePosition::fromSeconds ((q + 512) / sr)), true, false, true, false);
                            tw->applyToBuffer (fc);
                            for (int i = 0; i < 512; ++i) { o.l.push_back (buf.getSample (0, i)); o.r.push_back (buf.getSample (1, i)); }
                        }
                        tw->baseClassDeinitialise();
                    }
                    return o;
                };
                auto bin = [&] (const std::vector<float>& y, double f) { std::complex<double> acc = 0; const int from = 22050, M = 17640; if ((int) y.size() < from + M) return -300.0; for (int i = 0; i < M; ++i) acc += (double) y[(size_t) (from + i)] * std::polar (1.0, -twoPi * f * i / sr); return 20 * std::log10 (2 * std::abs (acc) / M + 1e-15); };
                auto peak = [] (const std::vector<float>& y) { float p = 0; for (size_t i = 8000; i < y.size(); ++i) p = std::max (p, std::abs (y[i])); return (double) p; };
                Set base; const float tiny = 0.0002f; const double tinyDb = 20 * std::log10 ((double) tiny);
                const auto quiet = render (base, tiny, 1000.0); const double got = bin (quiet.l, 1000.0) - tinyDb; const auto shownQ = SqVoltageOverdrivePlugin::measure (1234, 0, false, 50.0f);
                double corr = 0; for (int i = 22050; i < 22050 + 4410; ++i) corr += (double) quiet.l[(size_t) i] * std::sin (twoPi * 1000.0 * (i - lat) / sr);
                const double bass = bin (render (base, tiny, 80.0).l, 80.0) - tinyDb;
                check (std::abs (got - shownQ.gainDb) < 0.4 && got > 15.0 && corr > 0 && bass < got - 10.0 && std::abs (vp->getLatencySeconds() - 96.0 / 44100.0) < 1.0e-9 + 96.0 / 44100.0 * 0.2,
                       "a quiet 1 kHz tone comes out " + juce::String (got, 2) + " dB louder at Drive 50 % (the circuit's own arithmetic says " + juce::String (shownQ.gainDb, 2) + "), the right way up and 96 samples late as reported; 80 Hz only " + juce::String (bass, 2) + " dB - bass passes with less gain");
                double worstPeak = 0, leastPeak = 10; for (float dr : { 0.0f, 50.0f, 100.0f }) for (int dk = 0; dk < 3; ++dk) for (int sy = 0; sy < 2; ++sy) { Set s2 = base; s2.drive = dr; s2.diodes = dk; s2.sym = sy; const double p = peak (render (s2, 1.0f, 1000.0).l); worstPeak = std::max (worstPeak, p); leastPeak = std::min (leastPeak, p); }
                check (worstPeak < 1.12 && leastPeak > 0.4, "a full-scale 1 kHz tone at Drive 0, 50 and 100 % through every pair, even and lopsided, peaks between " + juce::String (20 * std::log10 (leastPeak), 2) + " and " + juce::String (20 * std::log10 (worstPeak), 2) + " dBFS before Output (the input plus what the diodes allow - it is not a limiter)");
                const auto h = render (base, 0.25f, 1000.0); const double h3 = bin (h.l, 3000.0) - bin (h.l, 1000.0), h2 = bin (h.l, 2000.0) - bin (h.l, 1000.0);
                Set lop = base; lop.sym = 1; const auto hl = render (lop, 0.25f, 1000.0); const double l2 = bin (hl.l, 2000.0) - bin (hl.l, 1000.0);
                Set lowD = base; lowD.drive = 0.0f; Set highD = base; highD.drive = 100.0f; const auto q0 = render (lowD, 0.01f, 1000.0), q1 = render (highD, 0.01f, 1000.0);
                const double d3lo = bin (q0.l, 3000.0) - bin (q0.l, 1000.0), d3hi = bin (q1.l, 3000.0) - bin (q1.l, 1000.0);
                check (h3 > -25.0 && h2 < -70.0 && l2 > -40.0 && d3hi > d3lo + 10.0, "a -12 dBFS tone at Drive 50 %: 3rd harmonic " + juce::String (h3, 1) + " dBc and no 2nd (" + juce::String (juce::roundToInt (h2)) + " dBc) with an even pair; Lopsided brings the 2nd up to " + juce::String (l2, 1) + " dBc; a -40 dBFS tone has its 3rd at " + juce::String (d3lo, 1) + " dBc with Drive at 0 and " + juce::String (d3hi, 1) + " at 100 %");
                Set softS = base; softS.diodes = 1; Set sharpS = base; sharpS.diodes = 2; const auto hs = render (softS, 0.25f, 1000.0), hh = render (sharpS, 0.25f, 1000.0);
                const double f5 = bin (h.l, 5000.0) - bin (h.l, 1000.0), s5 = bin (hs.l, 5000.0) - bin (hs.l, 1000.0), k5 = bin (hh.l, 5000.0) - bin (hh.l, 1000.0);
                Set dark = base; dark.tone = 0.0f; const double t10 = bin (render (dark, tiny, 10000.0).l, 10000.0) - bin (render (base, tiny, 10000.0).l, 10000.0);
                check (std::abs (s5 - f5) > 0.3 && std::abs (k5 - f5) > 0.3 && t10 < -12.0, "the three diode pairs clip differently: 5th harmonic " + juce::String (f5, 1) + " (silicon), " + juce::String (s5, 1) + " (soft), " + juce::String (k5, 1) + " dBc (sharp); Tone at its darkest takes 10 kHz down " + juce::String (-t10, 1) + " dB");
                Set dryS = base; dryS.mix = 0.0f; const auto dry = render (dryS, 0.25f, 1000.0); double worstDry = 0; for (int i = 2000; i < 30000; ++i) worstDry = std::max (worstDry, std::abs ((double) dry.l[(size_t) i] - 0.25 * std::sin (twoPi * 1000.0 * (i - lat) / sr)));
                Set other = base; other.unit = 99999; const auto ho = render (other, 0.25f, 1000.0), again = render (base, 0.25f, 1000.0);
                double same = 0; for (size_t i = 0; i < again.l.size(); ++i) same = std::max (same, (double) std::abs (again.l[i] - h.l[i]));
                const double dUnit = std::abs ((bin (ho.l, 5000.0) - bin (ho.l, 1000.0)) - f5), dLR = std::abs ((bin (h.r, 5000.0) - bin (h.r, 1000.0)) - f5);
                Set hq = base; hq.quality = 2; const auto h16 = render (hq, 0.25f, 1000.0); const auto shown = SqVoltageOverdrivePlugin::measure (1234, 0, false, 50.0f, 0.25f);
                check (worstDry < 1.0e-5 && dUnit > 0.0005 && dUnit < 3.0 && dLR > 0.0005 && dLR < 3.0 && same < 1.0e-6 && std::abs ((bin (h16.l, 3000.0) - bin (h16.l, 1000.0)) - h3) < 0.3 && std::abs (shown.harmonics[1] - h3) < 1.5 && shown.wave.size() == 384 && std::abs (shown.bassHz - 720.0) < 60.0,
                       "Mix at 0 % is the input delayed by the same 96 samples; two units differ slightly (" + juce::String (dUnit, 2) + " dB in the 5th harmonic), left and right are two circuits (" + juce::String (dLR, 2) + " dB), the same unit is the same every time, 16x gives the same 3rd harmonic as 8x, and the face's own figure for it is " + juce::String (shown.harmonics[1], 1) + " dB against " + juce::String (h3, 1) + " in the audio");
                int nPre = 0, nAdv = 0; for (auto& e2 : Signature::entries()) if (juce::String (e2.type) == "sqmVoltageOverdrive") { nPre = (int) e2.presets.size(); for (auto& pr : e2.presets) nAdv += pr.advanced ? 1 : 0; }
                const auto pp = call ("plugin_presets", R"({"track":"Audio 1","slot":"Voltage Overdrive","preset":"Bass Grit"})");
                const bool presetOk = pp.contains ("Bass Grit") && juce::roundToInt (vp->drive->getCurrentValue()) == 35 && juce::roundToInt (vp->mix->getCurrentValue()) == 60;
                call ("set_plugin_parameters", R"({"track":"Audio 1","slot":"Voltage Overdrive","params":[{"param":"Diodes","text":"Sharp"},{"param":"Symmetry","text":"Lopsided"},{"param":"Drive","text":"70"},{"param":"Quality","text":"16x"}]})");
                check (nPre >= 10 && nAdv >= 3 && nAdv < nPre && presetOk && vp->diodes->getCurrentValueAsString() == "Sharp" && vp->symmetry->getCurrentValueAsString() == "Lopsided" && juce::roundToInt (vp->drive->getCurrentValue()) == 70 && juce::roundToInt (vp->quality->getCurrentValue()) == 2
                       && Edition::packOf ("sqmVoltageOverdrive") == Edition::Pack::color && Skins::artFor ("voltageoverdrive").isValid(),
                       "Voltage Overdrive has " + juce::String (nPre) + " presets (" + juce::String (nAdv) + " advanced), belongs to Color & Space, has its plate, a preset sets Drive and Mix, and the assistant sets Diodes, Symmetry, Drive and Quality by name");
            }
        }

        // ---- a missing audio file: renders say so instead of waiting for ever
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sq-missing-selftest"); dir.deleteRecursively(); dir.createDirectory();
            const auto f = dir.getChildFile ("Gone Later.wav");
            {
                juce::AudioBuffer<float> b (1, 48000); for (int i = 0; i < 48000; ++i) b.getWritePointer (0)[i] = (float) (0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * i / 48000.0));
                juce::WavAudioFormat wav;
                if (auto os = std::make_unique<juce::FileOutputStream> (f); os->openedOk())
                    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), 48000.0, 1, 24, {}, 0))) { os.release(); w->writeFromAudioSampleBuffer (b, 0, 48000); }
            }
            const auto r = call ("add_track", R"({"kind":"audio","name":"Gone Later"})"); int idx = 0;
            for (int i = 0; i < r.length(); ++i) if (juce::CharacterFunctions::isDigit (r[i])) { idx = r.substring (i).getIntValue(); break; }
            call ("import_audio", "{\"track\":" + juce::String (idx) + ",\"start_seconds\":0,\"path\":" + juce::JSON::toString (juce::var (f.getFullPathName())) + "}");
            const bool gone = f.deleteFile();
            const auto b1 = call ("bounce", R"({"name":"st missing","start_seconds":0,"end_seconds":2})", false);
            check (gone && b1.contains ("is missing") && b1.contains ("Gone Later.wav"), "bounce with a missing audio file stops and names the file - " + b1.substring (0, 110));
            const auto b2 = call ("session_advice", R"({"start_seconds":0,"end_seconds":2})", false);
            check (b2.contains ("is missing") && b2.contains ("Gone Later"), "session_advice does the same");
            const auto b3 = call ("analyze_mix", R"({"start_seconds":0,"end_seconds":2})", false);
            check (b3.contains ("is missing"), "analyze_mix does the same");
            const auto b4 = call ("video_export", R"({"picture":"black","start_seconds":0,"end_seconds":2})", false);
            check (b4.contains ("is missing") || b4.contains ("action install"), "video_export does the same (or reports that the video tools are not installed)");
            call ("delete_track", "{\"track\":" + juce::String (idx) + "}");
            const auto b5 = call ("analyze_mix", R"({"start_seconds":0,"end_seconds":2})");
            check (b5.contains ("peak_dbfs"), "with the clip removed, measuring works again");
            dir.deleteRecursively();
        }
        std::cout << "  (1.15 Reference section took " << juce::String ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0, 1) << " s)" << std::endl;
    }

    // ===================================================================== Phase Z: numbers on every generic face
    {
        std::cout << "\n-- Phase Z: range numbers and meters on every face --\n";
        check (Skins::compactValue ("20000.0 Hz") == "20k" && Skins::compactValue ("-24.00 dB") == "-24" && Skins::compactValue ("2500 ms") == "2.5s"
               && Skins::compactValue ("100.0 %") == "100%" && Skins::compactValue ("4.00:1") == "4:1" && Skins::compactValue ("Off") == "Off",
               "Knob scale text: 20000.0 Hz -> 20k, -24.00 dB -> -24, 2500 ms -> 2.5s, 100.0 % -> 100%, 4.00:1 -> 4:1");
        call ("new_session", R"({"name":"Selftest Faces"})");
        call ("add_track", R"({"kind":"audio","name":"Faces"})");
        int faces = 0, scaled = 0, metered = 0;
        for (auto nm : { "Cocoon Compressor", "Ember Tape", "Deep Sub", "Helix Pitch" })
        {
            juce::String why; auto np = session->createPluginByName (nm, why);
            if (np == nullptr) continue;
            if (auto tt = session->findTrack ("Faces")) tt->pluginList.insertPlugin (np, 0, nullptr);
            Skins::SkinEditor ed (*np); ++faces;
            if (ed.getWidth() > Skins::specFor (nm).width) ++metered;
            bool anyScale = false; for (auto* c : ed.getChildren()) if (auto* k = dynamic_cast<Skins::Knob*> (c)) anyScale = anyScale || (bool) k->getHandle().endText;
            scaled += anyScale ? 1 : 0;
            np->deleteFromParent();
        }
        check (faces == 4 && scaled == 4 && metered == 4, "Generic faces: " + juce::String (scaled) + " of " + juce::String (faces) + " print knob ranges, " + juce::String (metered) + " carry the L / R meter column");
    }

    // ===================================================================== Phase Z: the main windows, drawn offscreen for inspection (/tmp/main_*.png)
    {
        std::cout << "\n-- Phase Z: main windows offscreen --\n";
        call ("new_session", R"({"name":"Selftest Windows"})");
        for (auto t : { "Kick", "Bass", "Keys", "Vocal" }) call ("add_track", juce::String (R"({"kind":"audio","name":")") + t + "\"}");
        auto shoot = [&] (juce::Component& c, int w, int h, const juce::String& file)
        {
            c.setSize (w, h); juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            auto snap = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
            juce::File f ("/tmp/main_" + file + ".png"); f.deleteFile();
            { juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (snap, os); }
            return snap.isValid() && snap.getWidth() == w;
        };
        TimelineView tl (*session); MixerView mx (*session, &tl.state); BeatPage bp (*session); MasteringProject mp (*session); MasteringContent mc (mp);
        const bool ok = shoot (tl, 1600, 900, "edit") && shoot (mx, 1600, 900, "mix") && shoot (bp, 1600, 900, "beat") && shoot (mc, 1600, 900, "mastering");
        check (ok, "Edit, Mix, Beat page and Mastering windows render offscreen at 1600 x 900");
        // Squelchmoth Chat in both providers (the CLAUDE / CHATGPT switch)
        const auto savedProvider = Prefs::getString ("chat_provider");
        Prefs::set ("chat_provider", "claude");
        bool chatOk;
        { Chat::Panel cp (*session, tools); chatOk = shoot (cp, 460, 900, "chat_claude"); }
        Prefs::set ("chat_provider", "chatgpt");
        { Chat::Panel cp (*session, tools); chatOk = shoot (cp, 460, 900, "chat_chatgpt") && chatOk && cp.getEngine().provider == Chat::Provider::chatgpt; }
        Prefs::set ("chat_provider", savedProvider.isNotEmpty() ? savedProvider : juce::String ("claude"));
        check (chatOk, "Squelchmoth Chat renders with Claude and with ChatGPT selected (the panel follows the saved switch)");
        // Channel Rack (a kit + an instrument with steps) and Score (a MIDI clip with a melody)
        call ("rack", R"({"action":"add","type":"instrument","what":"Venom","name":"Lead"})");
        call ("rack", R"({"action":"add","type":"kit","what":"Trap"})");
        call ("rack", R"({"action":"write","channel":"Kick","steps":"x...x...x...x..."})");
        call ("rack", R"({"action":"write","channel":"Snare","steps":"....x.......x..."})");
        call ("add_track", R"({"kind":"midi","name":"Melody"})");
        call ("create_midi_clip", R"({"track":"Melody","start_bar":1,"length_bars":2,"name":"Tune","notes":[
            {"pitch":"C4","start_beat":0,"length_beats":1},{"pitch":"E4","start_beat":1,"length_beats":1},{"pitch":"G4","start_beat":2,"length_beats":0.5},
            {"pitch":"A4","start_beat":2.5,"length_beats":0.5},{"pitch":"G4","start_beat":3,"length_beats":1},{"pitch":"F4","start_beat":4,"length_beats":2},
            {"pitch":"D4","start_beat":6,"length_beats":1},{"pitch":"C4","start_beat":7,"length_beats":1}]})");
        ChannelRackComponent rk (*session); ScoreContent sc (*session);
        bool rackOk = shoot (rk, 1400, 760, "rack");
        rk.setGraphVisible (true);
        rackOk = shoot (rk, 1400, 760, "rack_graph") && rackOk;
        check (rackOk && shoot (sc, 1600, 900, "score"), "Channel Rack (with and without the Graph Editor) and Score render offscreen with content");
        // dialogs: Bounce, Export Stems, New Tracks, the EQ frequency chart and the Claude / ChatGPT connect panel
        {
            BounceExport::Dialog bd (*session, false, [] (te::Track*) { return true; }, true, std::nullopt);
            BounceExport::Dialog sd (*session, true, [] (te::Track*) { return true; }, true, std::nullopt);
            NewTracksContent nt (*session, nullptr);
            EQChartContent eqc;
            Tunnel tunnelForShot;
            ClaudePanel cpn (*session, server, tunnelForShot, props);
            const bool dOk = shoot (bd, bd.getWidth(), bd.getHeight(), "dlg_bounce")
                          && shoot (sd, sd.getWidth(), sd.getHeight(), "dlg_stems")
                          && shoot (nt, juce::jmax (620, nt.getWidth()), juce::jmax (300, nt.getHeight()), "dlg_newtracks")
                          && shoot (eqc, 1400, 860, "dlg_eqchart")
                          && shoot (cpn, 1500, 900, "dlg_connect");
            check (dOk, "Bounce, Export Stems, New Tracks, EQ chart and the connect panel render offscreen");
        }
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
