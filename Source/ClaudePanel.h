#pragma once
// Squelchmoth - the Claude panel: a guided connection wizard with live status lights, the public link,
// one-click copy, provider fallback, a self-test, a diagnostics export and the live log of what
// Claude does inside the DAW.

#include "McpServer.h"
#include "Tunnel.h"
#include "Theme.h"

class ClaudePanel : public juce::Component,
                    private juce::ChangeListener,
                    private juce::Timer
{
public:
    static constexpr const char* connectorsPage = "https://claude.ai/settings/connectors";

    ClaudePanel (Session& s, McpServer& srv, Tunnel& t, juce::PropertiesFile& p)
        : session (s), server (srv), tunnel (t), props (p)
    {
        heading.setText ("Let Claude or ChatGPT drive Squelchmoth", juce::dontSendNotification);
        heading.setFont (Theme::font (24.0f, true));
        heading.setColour (juce::Label::textColourId, Theme::acid());
        addAndMakeVisible (heading);

        sub.setText ("Claude (claude.ai) or ChatGPT (chatgpt.com) controls every track, clip, plugin and transport function through a private link only this computer answers.", juce::dontSendNotification);
        sub.setFont (Theme::font (13.0f));
        sub.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (sub);

        for (int i = 0; i < 5; ++i)
        {
            addAndMakeVisible (stepLabels[i]);
            stepLabels[i].setFont (Theme::font (14.0f));
        }

        steps.setMultiLine (true);
        steps.setReadOnly (true);
        steps.setFont (Theme::font (14.0f));
        steps.setColour (juce::TextEditor::backgroundColourId, Theme::panel());
        steps.setColour (juce::TextEditor::outlineColourId, Theme::line());
        steps.setText (
            "HOW TO CONNECT (one time)\n"
            "1. Press  CONNECT CLAUDE  and wait for the lights above to turn green (5-20 seconds).\n"
            "2. Press  COPY LINK.\n"
            "3. Press  OPEN CLAUDE CONNECTORS  - it opens claude.ai > Settings > Connectors in your browser.\n"
            "4. Click  Add custom connector.  Name: Squelchmoth.  Paste the link in the URL field. No sign-in / OAuth. Click Add.\n"
            "5. In a chat, click the  +  (or tools) button, open Connectors, switch Squelchmoth on and ask:\n"
            "    \"Use Squelchmoth: make a 92 BPM lo-fi beat with keys, bass and drums, then bounce it.\"\n\n"
            "Keep Squelchmoth open while Claude works. With Cloudflare or Pinggy the link address changes each time you\n"
            "connect, so after a restart edit the connector on claude.ai and paste the new link (ngrok keeps a fixed address).\n"
            "Custom connectors need a Claude Pro, Max, Team or Enterprise plan.\n\n"
            "CHATGPT INSTEAD (same link, same full control)\n"
            "1. Do steps 1 and 2 above (connect, copy the link).\n"
            "2. Press  OPEN CHATGPT GUIDE  - OpenAI's page on adding your own MCP server to ChatGPT (it needs Developer mode,\n"
            "    on a paid ChatGPT plan, on the web).\n"
            "3. Following that page, turn Developer mode on and create a new app / connector: Name: Squelchmoth, MCP server URL: the\n"
            "    link you copied, authentication: none. Save it.\n"
            "4. In a new ChatGPT chat, open the  +  menu, choose Developer mode and select Squelchmoth, then ask what you want.\n"
            "    ChatGPT asks you to approve actions that change the session before it runs them.\n"
            "Inside the app, Squelchmoth Chat (the chat panel) has a CLAUDE / CHATGPT switch: same tools, your own API key for each.");
        addAndMakeVisible (steps);

        connectBtn.setColour (juce::TextButton::buttonColourId, Theme::violet());
        connectBtn.onClick = [this] { tunnel.isActive() ? tunnel.stopTunnel() : tunnel.startTunnel (server.getPort()); update(); };
        addAndMakeVisible (connectBtn);

        copyBtn.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (getConnectorUrl()); copyBtn.setButtonText ("COPIED"); juce::Timer::callAfterDelay (1500, [sp = juce::Component::SafePointer<juce::TextButton> (&copyBtn)] { if (sp) sp->setButtonText ("COPY LINK"); }); };
        addAndMakeVisible (copyBtn);

        openBtn.onClick = [] { juce::URL (connectorsPage).launchInDefaultBrowser(); };
        addAndMakeVisible (openBtn);

        testBtn.onClick = [this] { runSelfTest(); };
        addAndMakeVisible (testBtn);

        diagBtn.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard (diagnosticsText());
            diagBtn.setButtonText ("COPIED - paste to Claude");
            juce::Timer::callAfterDelay (2500, [sp = juce::Component::SafePointer<juce::TextButton> (&diagBtn)] { if (sp) sp->setButtonText ("COPY DIAGNOSTICS"); });
        };
        addAndMakeVisible (diagBtn);

        gptBtn.setTooltip ("help.openai.com: connecting your own MCP server in ChatGPT (Developer mode)");
        gptBtn.onClick = [] { juce::URL ("https://help.openai.com/en/articles/12584461-developer-mode-and-mcp-apps-in-chatgpt").launchInDefaultBrowser(); };
        addAndMakeVisible (gptBtn);

        provider.addItem ("Cloudflare quick tunnel (default, no account)", 1);
        provider.addItem ("ngrok (free account, fixed address, strict networks)", 2);
        provider.addItem ("Pinggy via SSH on port 443 (no account, 60 min)", 3);
        auto prov = Prefs::tunnelProvider();
        provider.setSelectedId (prov == "ngrok" ? 2 : prov == "pinggy" ? 3 : 1, juce::dontSendNotification);
        provider.onChange = [this]
        {
            const char* keys[] = { "cloudflare", "ngrok", "pinggy" };
            Prefs::set ("cl_provider", keys[juce::jlimit (0, 2, provider.getSelectedId() - 1)]);
            if (provider.getSelectedId() == 2 && Prefs::ngrokToken().isEmpty()) askNgrokToken();
            if (tunnel.isActive()) { tunnel.stopTunnel(); tunnel.startTunnel (server.getPort()); }
            update();
        };
        addAndMakeVisible (provider);
        providerLabel.setText ("Link provider:", juce::dontSendNotification);
        providerLabel.setFont (Theme::font (12.0f));
        providerLabel.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (providerLabel);

        newSecretBtn.onClick = [this]
        {
            server.regenerateToken();
            server.stop();
            server.start();
            session.log.add ("Squelchmoth", "New secret created. Old Claude links no longer work; copy the new one.");
            update();
        };
        addAndMakeVisible (newSecretBtn);

        autoConnect.setToggleState (Prefs::claudeAutoConnect(), juce::dontSendNotification);
        autoConnect.onClick = [this] { Prefs::set ("cl_autoConnect", autoConnect.getToggleState()); };
        addAndMakeVisible (autoConnect);

        urlBox.setReadOnly (true);
        urlBox.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold)));
        urlBox.setColour (juce::TextEditor::textColourId, Theme::mint());
        addAndMakeVisible (urlBox);

        localUrl.setReadOnly (true);
        localUrl.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        addAndMakeVisible (localUrl);
        localLabel.setText ("Local link (Claude Desktop / Claude Code on this Mac - no internet link needed):", juce::dontSendNotification);
        localLabel.setColour (juce::Label::textColourId, Theme::dim());
        localLabel.setFont (Theme::font (11.5f));
        addAndMakeVisible (localLabel);

        logTitle.setText ("WHAT CLAUDE IS DOING", juce::dontSendNotification);
        logTitle.setFont (Theme::font (12.0f, true));
        logTitle.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (logTitle);

        log.setMultiLine (true);
        log.setReadOnly (true);
        log.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        log.setColour (juce::TextEditor::backgroundColourId, Theme::bg());
        addAndMakeVisible (log);

        tunnel.addChangeListener (this);
        session.log.addChangeListener (this);
        update();
        refreshLog();
        startTimerHz (2);
    }

    ~ClaudePanel() override
    {
        tunnel.removeChangeListener (this);
        session.log.removeChangeListener (this);
    }

    juce::String getConnectorUrl() const
    {
        auto base = tunnel.getPublicBase();
        return base.isNotEmpty() ? base + server.getPath() : juce::String();
    }

    std::function<void (const juce::String&, bool)> onStatus;

    /** Checks the whole chain: local server answers MCP initialize, link registered, public /health reachable. */
    void runSelfTest()
    {
        juce::String report;
        // 1. local protocol round trip
        juce::String sid; bool notif = false;
        auto reply = server.handleBody (R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"selftest","version":"1"}}})", sid, notif);
        bool initOk = reply.contains ("squelchmoth") && reply.contains ("protocolVersion");
        auto list = server.handleBody (R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})", sid, notif);
        auto nTools = juce::JSON::parse (list)["result"]["tools"].size();
        report << (initOk ? "PASS" : "FAIL") << "  MCP protocol: initialize answered, " << nTools << " tools listed\n";
        report << (server.isRunning() ? "PASS" : "FAIL") << "  Control server listening on 127.0.0.1:" << server.getPort() << "\n";
        auto bundled = Tunnel::bundledCloudflared();
        report << (bundled.existsAsFile() ? "PASS" : "INFO") << "  Link tool: " << (bundled.existsAsFile() ? "cloudflared bundled inside the app" : "cloudflared not bundled - will download on first connect") << "\n";
        auto stage = tunnel.getStage();
        report << (stage == Tunnel::Stage::reachable ? "PASS" : stage == Tunnel::Stage::registered ? "WARN" : "FAIL") << "  Public link: " << tunnel.getState() << "\n";
        if (stage == Tunnel::Stage::registered || stage == Tunnel::Stage::reachable) tunnel.requestReachabilityTest();
        report << (server.getRequestCount() > 0 ? "PASS" : "INFO") << "  claude.ai has " << (server.getRequestCount() > 0 ? "already called in (" + juce::String (server.getRequestCount()) + " requests)" : "not called in yet - add the connector on claude.ai") << "\n";
        if (stage == Tunnel::Stage::failed || stage == Tunnel::Stage::off)
            report << "\nNEXT STEP: press CONNECT CLAUDE. If it fails on this network, choose ngrok in the provider menu (free account) - it uses the normal web port.";
        else if (stage == Tunnel::Stage::reachable)
            report << "\nEverything on this Mac is working. If claude.ai still says it cannot connect, make sure you pasted the full link (ending in the long secret) and chose no authentication.";
        session.log.add ("Self-test", report.replace ("\n", "  |  "), ! report.contains ("FAIL"));
        Ui::message (report.contains ("FAIL") ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon, "Claude connection self-test", report);
    }

    juce::String diagnosticsText() const
    {
        juce::String s;
        s << "Squelchmoth " << JUCE_APPLICATION_VERSION_STRING << " diagnostics  " << juce::Time::getCurrentTime().toString (true, true) << "\n";
        s << "OS: " << juce::SystemStats::getOperatingSystemName() << "  CPU: " << juce::SystemStats::getCpuModel() << "\n";
        s << "Server running: " << (server.isRunning() ? "yes" : "no") << "  port " << server.getPort() << "  requests " << server.getRequestCount() << "  last error: " << server.getLastError() << "\n";
        s << "Provider: " << Prefs::tunnelProvider() << "  stage: " << (int) tunnel.getStage() << "  state: " << tunnel.getState() << "\n";
        s << "Public base: " << tunnel.getPublicBase() << "\n";
        s << "Bundled cloudflared: " << (Tunnel::bundledCloudflared().existsAsFile() ? "yes" : "no") << "\n\n--- link log ---\n" << tunnel.getDiagnostics() << "\n\n--- activity ---\n";
        auto entries = session.log.getEntries();
        for (size_t i = entries.size() > 40 ? entries.size() - 40 : 0; i < entries.size(); ++i)
            s << entries[i].time.formatted ("%H:%M:%S") << " [" << entries[i].source << "] " << entries[i].text << "\n";
        return s;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg());
        // status lights
        for (int i = 0; i < 5; ++i)
        {
            auto r = stepLabels[i].getBounds();
            auto light = juce::Rectangle<float> ((float) r.getX() - 22, (float) r.getCentreY() - 7, 14, 14);
            g.setColour (lights[i] == 2 ? Theme::mint() : lights[i] == 1 ? Theme::amber() : Theme::line());
            g.fillEllipse (light);
            if (lights[i] == 2) { g.setColour (Theme::bg()); g.drawLine (light.getX() + 3, light.getCentreY(), light.getCentreX() - 1, light.getBottom() - 4, 2.0f); g.drawLine (light.getCentreX() - 1, light.getBottom() - 4, light.getRight() - 3, light.getY() + 3, 2.0f); }
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 16);
        auto left = r.removeFromLeft (juce::jmin (820, r.getWidth() * 3 / 5));
        r.removeFromLeft (20);

        heading.setBounds (left.removeFromTop (34));
        sub.setBounds (left.removeFromTop (22));
        left.removeFromTop (8);
        for (int i = 0; i < 5; ++i) stepLabels[i].setBounds (left.removeFromTop (22).withTrimmedLeft (28));
        left.removeFromTop (10);

        auto row = left.removeFromTop (40);
        connectBtn.setBounds (row.removeFromLeft (200)); row.removeFromLeft (8);
        copyBtn.setBounds (row.removeFromLeft (120)); row.removeFromLeft (8);
        openBtn.setBounds (row.removeFromLeft (230)); row.removeFromLeft (8);
        testBtn.setBounds (row.removeFromLeft (110));
        left.removeFromTop (8);
        urlBox.setBounds (left.removeFromTop (32));
        left.removeFromTop (8);
        auto prow = left.removeFromTop (26);
        providerLabel.setBounds (prow.removeFromLeft (90));
        provider.setBounds (prow.removeFromLeft (360)); prow.removeFromLeft (10);
        autoConnect.setBounds (prow);
        left.removeFromTop (10);
        steps.setBounds (left.removeFromTop (juce::jmax (150, left.getHeight() - 120)));
        left.removeFromTop (8);
        localLabel.setBounds (left.removeFromTop (18));
        localUrl.setBounds (left.removeFromTop (24));
        left.removeFromTop (6);
        auto brow = left.removeFromTop (26);
        newSecretBtn.setBounds (brow.removeFromLeft (220)); brow.removeFromLeft (8);
        diagBtn.setBounds (brow.removeFromLeft (220)); brow.removeFromLeft (8);
        gptBtn.setBounds (brow.removeFromLeft (200));

        logTitle.setBounds (r.removeFromTop (22));
        log.setBounds (r);
    }

    void update()
    {
        bool srv = server.isRunning();
        auto stage = tunnel.getStage();
        auto url = getConnectorUrl();
        bool bundled = Tunnel::bundledCloudflared().existsAsFile() || Tunnel::getToolsDir().getChildFile ("cloudflared").existsAsFile() || Prefs::tunnelProvider() != "cloudflare";

        lights[0] = srv ? 2 : 0;
        lights[1] = bundled ? 2 : (stage == Tunnel::Stage::preparing ? 1 : 0);
        lights[2] = url.isNotEmpty() ? 2 : (stage == Tunnel::Stage::starting || stage == Tunnel::Stage::preparing ? 1 : 0);
        lights[3] = stage == Tunnel::Stage::reachable ? 2 : (stage == Tunnel::Stage::registered || stage == Tunnel::Stage::linkCreated ? 1 : 0);
        lights[4] = server.getRequestCount() > 0 ? 2 : (stage == Tunnel::Stage::reachable ? 1 : 0);

        stepLabels[0].setText (srv ? "Control server running on this Mac (port " + juce::String (server.getPort()) + ")" : "Control server NOT running: " + server.getLastError(), juce::dontSendNotification);
        stepLabels[1].setText (bundled ? "Link tool ready (" + juce::String (Prefs::tunnelProvider()) + ")" : "Link tool: downloading cloudflared on first connect", juce::dontSendNotification);
        stepLabels[2].setText (url.isNotEmpty() ? "Public link created" : (tunnel.isActive() ? "Creating the public link..." : "Public link: press CONNECT CLAUDE"), juce::dontSendNotification);
        stepLabels[3].setText (stage == Tunnel::Stage::reachable ? "Verified: the internet can reach Squelchmoth through the link"
                             : stage == Tunnel::Stage::registered ? "Link is up - verifying reachability..."
                             : stage == Tunnel::Stage::linkCreated ? "Waiting for the link network to confirm..."
                             : stage == Tunnel::Stage::failed ? tunnel.getState() : "Reachability: not yet", juce::dontSendNotification);
        stepLabels[4].setText (server.getRequestCount() > 0 ? "claude.ai is talking to Squelchmoth (" + juce::String (server.getRequestCount()) + " requests)"
                             : "Add the connector on claude.ai (steps below) - this light turns green when Claude first calls in", juce::dontSendNotification);
        for (int i = 0; i < 5; ++i) stepLabels[i].setColour (juce::Label::textColourId, lights[i] == 2 ? Theme::text() : lights[i] == 1 ? Theme::amber() : (i == 3 && stage == Tunnel::Stage::failed ? Theme::red() : Theme::dim()));

        connectBtn.setButtonText (tunnel.isActive() ? "DISCONNECT" : "CONNECT CLAUDE");
        copyBtn.setEnabled (url.isNotEmpty());
        urlBox.setText (url.isNotEmpty() ? url : juce::String ("(press CONNECT CLAUDE - the link appears here)"), false);
        localUrl.setText (server.getLocalUrl(), false);

        auto st = tunnel.getState();
        if (onStatus) onStatus (stage == Tunnel::Stage::reachable ? "Claude link live" : st, stage == Tunnel::Stage::reachable || stage == Tunnel::Stage::registered);
        repaint();
    }

private:
    Session& session;
    McpServer& server;
    Tunnel& tunnel;
    juce::PropertiesFile& props;

    juce::Label heading, sub, localLabel, logTitle, providerLabel, stepLabels[5];
    int lights[5] = { 0, 0, 0, 0, 0 };
    juce::TextEditor steps, urlBox, localUrl, log;
    juce::TextButton connectBtn { "CONNECT CLAUDE" }, copyBtn { "COPY LINK" }, openBtn { "OPEN CLAUDE CONNECTORS" }, testBtn { "SELF-TEST" }, diagBtn { "COPY DIAGNOSTICS" }, gptBtn { "OPEN CHATGPT GUIDE" }, newSecretBtn { "New secret (revoke old links)" };
    juce::ComboBox provider;
    juce::ToggleButton autoConnect { "Connect automatically at launch" };
    size_t lastLogCount = 0;

    void askNgrokToken()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("ngrok authtoken", "1. Create a free account at https://ngrok.com\n2. Dashboard > Your Authtoken > copy\n3. Paste it here. (Optional: also claim your free static domain and enter it in Preferences > Claude.)", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("tok", "", "Authtoken");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Open ngrok.com", 2);
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int r)
        {
            aw->setVisible (false);
            if (r == 2) juce::URL ("https://dashboard.ngrok.com/get-started/your-authtoken").launchInDefaultBrowser();
            if (r == 1) Prefs::set ("cl_ngrokToken", aw->getTextEditorContents ("tok").trim());
        }));
    }

    void changeListenerCallback (juce::ChangeBroadcaster* b) override
    {
        if (b == &tunnel) update();
        else refreshLog();
    }

    void refreshLog()
    {
        auto entries = session.log.getEntries();
        if (entries.size() == lastLogCount) return;
        lastLogCount = entries.size();
        juce::String text;
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            text << it->time.formatted ("%H:%M:%S") << "  " << (it->ok ? "" : "!! ") << "[" << it->source << "] " << it->text << "\n";
        log.setText (text, false);
    }

    void timerCallback() override   { update(); }
};
