#pragma once
// Squelchmoth - built-in Model Context Protocol server (Streamable HTTP transport).
// Claude connects to  http://127.0.0.1:<port>/mcp/<secret>  (locally, or through the tunnel).

#include "McpTools.h"

#if JUCE_MAC
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wshadow"
 #pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#include "httplib.h"
#if JUCE_MAC
 #pragma clang diagnostic pop
#endif

#include <future>

class McpServer : private juce::Thread
{
public:
    static constexpr int defaultPort = 7383;
    static constexpr const char* protocolVersion = "2025-06-18";

    McpServer (McpTools& t, juce::PropertiesFile& props)
        : juce::Thread ("Squelchmoth MCP"), tools (t), settings (props)
    {
        token = settings.getValue ("mcpToken");
        if (token.length() < 24)
        {
            token = juce::Uuid().toString().removeCharacters ("-") + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
            settings.setValue ("mcpToken", token);
            settings.saveIfNeeded();
        }
        port = settings.getIntValue ("mcpPort", defaultPort);
    }

    ~McpServer() override   { stop(); }

    bool start()
    {
        if (isThreadRunning()) return true;
        server = std::make_unique<httplib::Server>();
        setupRoutes();
        startThread();

        for (int i = 0; i < 50 && ! server->is_running(); ++i)
            juce::Thread::sleep (20);
        return server->is_running();
    }

    void stop()
    {
        if (server != nullptr) server->stop();
        stopThread (3000);
        server.reset();
    }

    bool isRunning() const              { return server != nullptr && server->is_running(); }
    int getPort() const                 { return port; }
    juce::String getToken() const       { return token; }
    juce::String getPath() const        { return "/mcp/" + token; }
    juce::String getLocalUrl() const    { return "http://127.0.0.1:" + juce::String (port) + getPath(); }
    int getRequestCount() const         { return requestCount.load(); }
    juce::String getLastError() const   { return lastError; }

    void regenerateToken()
    {
        token = juce::Uuid().toString().removeCharacters ("-");
        settings.setValue ("mcpToken", token);
        settings.saveIfNeeded();
    }

    // Exposed for the self-test command line mode.
    juce::String handleBody (const juce::String& body, juce::String& sessionIdOut, bool& isNotificationOnly)
    {
        auto parsed = juce::JSON::parse (body);
        isNotificationOnly = false;

        if (auto arr = parsed.getArray())
        {
            juce::Array<juce::var> responses;
            for (auto& msg : *arr)
                if (auto r = handleMessage (msg, sessionIdOut); ! r.isVoid())
                    responses.add (r);
            if (responses.isEmpty()) { isNotificationOnly = true; return {}; }
            return juce::JSON::toString (juce::var (responses), true);
        }

        if (! parsed.isObject())
            return juce::JSON::toString (makeError (juce::var(), -32700, "Parse error"), true);

        auto r = handleMessage (parsed, sessionIdOut);
        if (r.isVoid()) { isNotificationOnly = true; return {}; }
        return juce::JSON::toString (r, true);
    }

private:
    McpTools& tools;
    juce::PropertiesFile& settings;
    std::unique_ptr<httplib::Server> server;
    juce::String token, lastError;
    int port = defaultPort;
    std::atomic<int> requestCount { 0 };

    void run() override
    {
        if (! server->listen ("127.0.0.1", port))
        {
            lastError = "Could not listen on port " + juce::String (port) + " (already in use?)";
            DBG (lastError);
        }
    }

    void setupRoutes()
    {
        server->set_payload_max_length (32 * 1024 * 1024);
        server->set_read_timeout (300, 0);
        server->set_write_timeout (300, 0);
        server->new_task_queue = [] { return new httplib::ThreadPool (4); };

        auto path = getPath().toStdString();

        server->Get ("/", [] (const httplib::Request&, httplib::Response& res)
        {
            res.set_content ("Squelchmoth is running. Claude connects to the /mcp/<secret> URL shown in the app's Claude panel.\n", "text/plain");
        });

        // Health page used by the link's reachability self-test (no secret needed, reveals nothing).
        server->Get ("/health", [this] (const httplib::Request&, httplib::Response& res)
        {
            res.set_header ("Cache-Control", "no-store");
            res.set_content ("{\"ok\":true,\"app\":\"squelchmoth\",\"version\":\"" + std::string (JUCE_APPLICATION_VERSION_STRING) + "\",\"tools\":" + std::to_string (tools.getTools().size()) + "}", "application/json");
        });

        // claude.ai probes these when adding a connector; answering 404 quickly means "no OAuth needed".
        for (auto wk : { "/.well-known/oauth-protected-resource", "/.well-known/oauth-authorization-server", "/.well-known/openid-configuration" })
            server->Get (wk, [] (const httplib::Request&, httplib::Response& res) { res.status = 404; res.set_content ("{}", "application/json"); });
        server->Post ("/register", [] (const httplib::Request&, httplib::Response& res) { res.status = 404; res.set_content ("{}", "application/json"); });

        server->Post (path, [this] (const httplib::Request& req, httplib::Response& res)
        {
            ++requestCount;
            juce::String sessionId = juce::String (req.get_header_value ("Mcp-Session-Id"));
            bool notificationOnly = false;
            auto out = handleBody (juce::String::fromUTF8 (req.body.data(), (int) req.body.size()), sessionId, notificationOnly);

            if (sessionId.isNotEmpty())
                res.set_header ("Mcp-Session-Id", sessionId.toStdString());

            res.set_header ("Cache-Control", "no-store");
            if (notificationOnly) { res.status = 202; return; }
            res.set_content (out.toStdString(), "application/json");
        });

        server->Get (path, [] (const httplib::Request& req, httplib::Response& res)
        {
            // A browser hitting the secret link gets a friendly page; an MCP client asking for the SSE stream gets 405 (allowed by the spec).
            if (req.get_header_value ("Accept").find ("text/event-stream") != std::string::npos)
            {
                res.status = 405;
                res.set_header ("Allow", "POST, DELETE");
                return;
            }
            res.set_content ("<html><body style='font-family:sans-serif;background:#0d0714;color:#ece6ff;padding:40px'><h2>Squelchmoth control link is working</h2>"
                             "<p>This address is for claude.ai or ChatGPT, not for a browser. Paste it into claude.ai &gt; Settings &gt; Connectors &gt; Add custom connector, or into a ChatGPT Developer mode app as its MCP server URL.</p></body></html>", "text/html");
        });

        server->Get (path + "/health", [] (const httplib::Request&, httplib::Response& res)
        {
            res.set_content ("{\"ok\":true,\"app\":\"squelchmoth\",\"secret\":\"valid\"}", "application/json");
        });

        server->Delete (path, [] (const httplib::Request&, httplib::Response& res) { res.status = 200; });

        server->Options (path, [] (const httplib::Request&, httplib::Response& res)
        {
            res.set_header ("Access-Control-Allow-Origin", "*");
            res.set_header ("Access-Control-Allow-Headers", "*");
            res.set_header ("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
            res.status = 204;
        });
    }

    static juce::var makeResult (const juce::var& id, const juce::var& result)
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("jsonrpc", "2.0");
        o->setProperty ("id", id);
        o->setProperty ("result", result);
        return juce::var (o);
    }

    static juce::var makeError (const juce::var& id, int code, const juce::String& message)
    {
        auto e = new juce::DynamicObject();
        e->setProperty ("code", code);
        e->setProperty ("message", message);
        auto o = new juce::DynamicObject();
        o->setProperty ("jsonrpc", "2.0");
        o->setProperty ("id", id);
        o->setProperty ("error", juce::var (e));
        return juce::var (o);
    }

    juce::var handleMessage (const juce::var& msg, juce::String& sessionId)
    {
        auto method = msg["method"].toString();
        auto id = msg["id"];
        bool isRequest = msg.getDynamicObject() != nullptr && msg.getDynamicObject()->hasProperty ("id");

        if (! isRequest)
            return {};   // notifications (initialized, cancelled...) need no response

        if (method == "initialize")
        {
            sessionId = juce::Uuid().toString();
            {   // ChatGPT (OpenAI) and Claude both connect here: remember which one this session is, for the activity log
                const auto client = (msg["params"]["clientInfo"]["name"].toString() + " " + msg["params"]["clientInfo"]["title"].toString()).toLowerCase();
                const std::lock_guard<std::mutex> lk (callerLock);
                callerBySession[sessionId] = client.contains ("openai") || client.contains ("chatgpt") ? "ChatGPT" : "Claude";
            }
            auto requested = msg["params"]["protocolVersion"].toString();
            static const juce::StringArray supported { "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05" };

            auto r = new juce::DynamicObject();
            r->setProperty ("protocolVersion", supported.contains (requested) ? requested : juce::String (protocolVersion));

            auto caps = new juce::DynamicObject();
            auto toolCaps = new juce::DynamicObject();
            toolCaps->setProperty ("listChanged", false);
            caps->setProperty ("tools", juce::var (toolCaps));
            r->setProperty ("capabilities", juce::var (caps));

            auto info = new juce::DynamicObject();
            info->setProperty ("name", "squelchmoth");
            info->setProperty ("title", "Squelchmoth DAW");
            info->setProperty ("version", JUCE_APPLICATION_VERSION_STRING);
            r->setProperty ("serverInfo", juce::var (info));
            r->setProperty ("instructions",
                "You are controlling Squelchmoth, a full DAW (digital audio workstation) running on the user's Mac. "
                "Start with get_session to see the tracks. Tracks are addressed by number (1-based) or name; plugin inserts by slot (1-based) or name; "
                "positions use bars (1-based, bar 1 = start) or seconds. Use screenshot to see the screen. "
                "Third-party VST3/AU plugins appear after scan_plugins. Every edit is undoable with undo.");
            return makeResult (id, juce::var (r));
        }

        if (method == "ping")
            return makeResult (id, juce::var (new juce::DynamicObject()));

        if (method == "tools/list")
        {
            juce::Array<juce::var> list;
            for (auto& t : tools.getTools())
            {
                auto o = new juce::DynamicObject();
                o->setProperty ("name", t.name);
                o->setProperty ("description", t.description);
                o->setProperty ("inputSchema", t.schema);
                list.add (juce::var (o));
            }
            auto r = new juce::DynamicObject();
            r->setProperty ("tools", list);
            return makeResult (id, juce::var (r));
        }

        if (method == "tools/call")
        {
            auto name = msg["params"]["name"].toString();
            auto args = msg["params"]["arguments"];
            juce::String who = "Claude";
            { const std::lock_guard<std::mutex> lk (callerLock); auto it = callerBySession.find (sessionId); if (it != callerBySession.end()) who = it->second; }
            auto result = callOnMessageThread (name, args, who);

            juce::Array<juce::var> content;
            if (result.text.isNotEmpty() || result.imageBase64.isEmpty())
            {
                auto t = new juce::DynamicObject();
                t->setProperty ("type", "text");
                t->setProperty ("text", result.text.length() > 140000 ? result.text.substring (0, 140000) + "\n...(truncated)" : result.text);
                content.add (juce::var (t));
            }
            if (result.imageBase64.isNotEmpty())
            {
                auto im = new juce::DynamicObject();
                im->setProperty ("type", "image");
                im->setProperty ("data", result.imageBase64);
                im->setProperty ("mimeType", result.imageMime);
                content.add (juce::var (im));
            }
            auto r = new juce::DynamicObject();
            r->setProperty ("content", content);
            r->setProperty ("isError", result.isError);
            return makeResult (id, juce::var (r));
        }

        if (method == "resources/list")   { auto r = new juce::DynamicObject(); r->setProperty ("resources", juce::Array<juce::var>()); return makeResult (id, juce::var (r)); }
        if (method == "prompts/list")     { auto r = new juce::DynamicObject(); r->setProperty ("prompts", juce::Array<juce::var>()); return makeResult (id, juce::var (r)); }

        return makeError (id, -32601, "Method not found: " + method);
    }

    std::mutex callerLock;
    std::map<juce::String, juce::String> callerBySession;

    ToolResult callOnMessageThread (const juce::String& name, const juce::var& args, const juce::String& who = "Claude")
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        { tools.caller = who; return tools.call (name, args); }

        auto promise = std::make_shared<std::promise<ToolResult>>();
        auto future = promise->get_future();

        juce::MessageManager::callAsync ([this, promise, name, args, who]
        {
            tools.caller = who;
            promise->set_value (tools.call (name, args));
        });

        if (future.wait_for (std::chrono::seconds (280)) != std::future_status::ready)
            return ToolResult::error ("Timed out waiting for the DAW (it may be busy, e.g. showing a dialog).");

        return future.get();
    }
};
