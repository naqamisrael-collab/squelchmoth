#pragma once
// Squelchmoth - the public link that lets claude.ai (which runs in the cloud) reach the DAW's local
// control server. Three providers, chosen in Preferences > Claude:
//   * Cloudflare quick tunnel (default, no account): the cloudflared binary is bundled inside the app
//     (Contents/Resources) so nothing has to be downloaded; tries QUIC first, then falls back to HTTP/2.
//   * ngrok (free account, fixed address): connects over normal HTTPS (port 443), so it works on
//     networks that block Cloudflare's port 7844.
//   * Pinggy over SSH on port 443 (no account, 60-minute sessions; the link changes every hour).
// After the link is up the app fetches its own /health page through the public address to prove that
// the internet really can reach it, auto-restarts the link if it drops, and keeps a diagnostics log.

#include <JuceHeader.h>
#include "Prefs.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <ws2tcpip.h>
#else
 #include <netdb.h>
#endif

class Tunnel : private juce::Thread,
               public juce::ChangeBroadcaster
{
public:
    // 2025.6.0 is the newest cloudflared whose macOS build still runs on Big Sur (min macOS 11.0);
    // later builds require macOS 15. Verified by reading the Mach-O LC_BUILD_VERSION of each release.
    static constexpr const char* pinnedVersion = "2025.6.0";

    enum class Stage { off, preparing, starting, linkCreated, registered, reachable, failed };

    Tunnel() : juce::Thread ("Squelchmoth tunnel") {}
    ~Tunnel() override  { stopTunnel(); }

    void startTunnel (int localPort)
    {
        if (isThreadRunning()) return;
        port = localPort;
        userStopped = false;
        setState (Stage::preparing, "Starting...", {});
        startThread();
    }

    void stopTunnel()
    {
        userStopped = true;
        signalThreadShouldExit();
        if (process != nullptr) process->kill();
        stopThread (6000);
        process.reset();
        setState (Stage::off, "Off", {});
    }

    bool isActive() const               { return isThreadRunning(); }
    Stage getStage() const              { const juce::ScopedLock sl (lock); return stage; }
    juce::String getPublicBase() const  { const juce::ScopedLock sl (lock); return publicUrl; }
    juce::String getState() const       { const juce::ScopedLock sl (lock); return state; }
    juce::String getProvider() const    { const juce::ScopedLock sl (lock); return providerInUse; }
    juce::String getDiagnostics() const { const juce::ScopedLock sl (lock); return diagnostics.joinIntoString ("\n"); }
    bool isReachable() const            { return getStage() == Stage::reachable; }

    /** The path (secret) the control server listens on, so the reachability test can hit /health. */
    juce::String healthPath { "/health" };

    static juce::File getToolsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Squelchmoth").getChildFile ("bin");
    }

    /** Where the bundled cloudflared lives inside the .app (Contents/Resources). */
    static juce::File bundledCloudflared()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getChildFile ("Contents/Resources/cloudflared");
       #elif JUCE_WINDOWS
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("cloudflared.exe");
       #else
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("cloudflared");
       #endif
    }

    /** Runs a reachability check from a background thread and reports through the state. */
    void requestReachabilityTest()     { recheckRequested = true; }

private:
    int port = 7383;
    std::unique_ptr<juce::ChildProcess> process;
    juce::CriticalSection lock;
    Stage stage = Stage::off;
    juce::String state { "Off" }, publicUrl, providerInUse;
    juce::StringArray diagnostics;
    std::atomic<bool> userStopped { false }, recheckRequested { false };

    void setState (Stage s, const juce::String& text, const juce::String& url)
    {
        {
            const juce::ScopedLock sl (lock);
            stage = s;
            state = text;
            publicUrl = url;
        }
        diag ("[state] " + text + (url.isNotEmpty() ? "  " + url : ""));
        sendChangeMessage();
    }

    void diag (const juce::String& line)
    {
        const juce::ScopedLock sl (lock);
        diagnostics.add (juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + line);
        if (diagnostics.size() > 400) diagnostics.removeRange (0, 100);
    }

    //==========================================================================
    bool bundleDistrusted = false;   // set when the bundled copy failed to run: fall back to a fresh download
    bool sawCloudflaredOutput = false; // did the helper print anything that looks like cloudflared log output?

    juce::File findOrInstallCloudflared()
    {
        auto bundled = bundledCloudflared();
        if (bundled.existsAsFile() && ! bundleDistrusted)
        {
            // run from a writable copy so Gatekeeper / translocation can't interfere
            auto dir = getToolsDir();
            dir.createDirectory();
            auto exe = dir.getChildFile (bundled.getFileName());
            if (! exe.existsAsFile() || exe.getSize() != bundled.getSize())
                bundled.copyFileTo (exe);
            exe.setExecutePermission (true);
            if (exe.existsAsFile()) return exe;
            return bundled;
        }

        for (auto p : { "/usr/local/bin/cloudflared", "/opt/homebrew/bin/cloudflared", "/usr/bin/cloudflared" })
            if (juce::File (p).existsAsFile())
                return juce::File (p);

        auto dir = getToolsDir();
        auto exe = dir.getChildFile (bundleDistrusted ? "cloudflared-dl" : "cloudflared");
        if (exe.existsAsFile()) return exe;

        dir.createDirectory();
       #if JUCE_MAC
              #if JUCE_ARM
        auto arch = "arm64";
      #else
        auto arch = "amd64";
      #endif
        juce::String url = juce::String ("https://github.com/cloudflare/cloudflared/releases/download/") + pinnedVersion
                            + "/cloudflared-darwin-" + arch + ".tgz";
        auto tgz = dir.getChildFile ("cloudflared.tgz");
        setState (Stage::preparing, "Downloading cloudflared " + juce::String (pinnedVersion) + "...", {});

        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "/usr/bin/curl", "-fsSL", "-o", tgz.getFullPathName(), url }))
            dl.waitForProcessToFinish (180000);

        juce::ChildProcess untar;
        if (untar.start (juce::StringArray { "/usr/bin/tar", "-xzf", tgz.getFullPathName(), "-C", dir.getFullPathName() }))
            untar.waitForProcessToFinish (60000);
        tgz.deleteFile();
        if (bundleDistrusted) { auto plain = dir.getChildFile ("cloudflared"); if (plain.existsAsFile()) { exe.deleteFile(); plain.moveFileTo (exe); } }
       #elif JUCE_WINDOWS
        exe = dir.getChildFile ("cloudflared.exe");
        if (exe.existsAsFile()) return exe;
        juce::String url = juce::String ("https://github.com/cloudflare/cloudflared/releases/download/") + pinnedVersion + "/cloudflared-windows-amd64.exe";
        setState (Stage::preparing, "Downloading cloudflared...", {});
        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "curl.exe", "-fsSL", "-o", exe.getFullPathName(), url }))
            dl.waitForProcessToFinish (180000);
       #else
        juce::String url = juce::String ("https://github.com/cloudflare/cloudflared/releases/download/") + pinnedVersion + "/cloudflared-linux-amd64";
        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "curl", "-fsSL", "-o", exe.getFullPathName(), url }))
            dl.waitForProcessToFinish (180000);
       #endif

        if (exe.existsAsFile())
            exe.setExecutePermission (true);
        return exe;
    }

    juce::File findOrInstallNgrok()
    {
        for (auto p : { "/usr/local/bin/ngrok", "/opt/homebrew/bin/ngrok", "/usr/bin/ngrok" })
            if (juce::File (p).existsAsFile()) return juce::File (p);
        auto dir = getToolsDir();
        dir.createDirectory();
       #if JUCE_WINDOWS
        auto exe = dir.getChildFile ("ngrok.exe");
       #else
        auto exe = dir.getChildFile ("ngrok");
       #endif
        if (exe.existsAsFile()) return exe;
       #if JUCE_MAC
        #if JUCE_ARM
        juce::String url = "https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-darwin-arm64.zip";
        #else
        juce::String url = "https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-darwin-amd64.zip";
        #endif
        auto zip = dir.getChildFile ("ngrok.zip");
        setState (Stage::preparing, "Downloading ngrok...", {});
        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "/usr/bin/curl", "-fsSL", "-o", zip.getFullPathName(), url })) dl.waitForProcessToFinish (180000);
        juce::ChildProcess unzip;
        if (unzip.start (juce::StringArray { "/usr/bin/unzip", "-o", zip.getFullPathName(), "-d", dir.getFullPathName() })) unzip.waitForProcessToFinish (60000);
        zip.deleteFile();
       #elif JUCE_WINDOWS
        juce::String url = "https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-windows-amd64.zip";
        auto zip = dir.getChildFile ("ngrok.zip");
        setState (Stage::preparing, "Downloading ngrok...", {});
        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "curl.exe", "-fsSL", "-o", zip.getFullPathName(), url })) dl.waitForProcessToFinish (180000);
        juce::ChildProcess unzip;
        if (unzip.start (juce::StringArray { "tar.exe", "-xf", zip.getFullPathName(), "-C", dir.getFullPathName() })) unzip.waitForProcessToFinish (60000);
        zip.deleteFile();
       #else
        juce::String url = "https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-amd64.tgz";
        auto tgz = dir.getChildFile ("ngrok.tgz");
        juce::ChildProcess dl;
        if (dl.start (juce::StringArray { "curl", "-fsSL", "-o", tgz.getFullPathName(), url })) dl.waitForProcessToFinish (180000);
        juce::ChildProcess untar;
        if (untar.start (juce::StringArray { "tar", "-xzf", tgz.getFullPathName(), "-C", dir.getFullPathName() })) untar.waitForProcessToFinish (60000);
        tgz.deleteFile();
       #endif
        if (exe.existsAsFile()) exe.setExecutePermission (true);
        return exe;
    }

    //==========================================================================
    /** True when the internet is reachable (a DNS lookup of the tunnel service answers). Squelchmoth itself
        never needs the internet - only the Claude link does. */
    static bool internetAvailable()
    {
        for (auto host : { "api.trycloudflare.com", "www.apple.com" })
        {
            addrinfo hints {}, *res = nullptr;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo (host, "443", &hints, &res) == 0 && res != nullptr) { freeaddrinfo (res); return true; }
        }
        return false;
    }

    void run() override
    {
        auto provider = Prefs::tunnelProvider();
        { const juce::ScopedLock sl (lock); providerInUse = provider; }
        int attempt = 0;

        // Offline: wait quietly (everything else in Squelchmoth works offline) and connect when the network returns.
        while (! threadShouldExit() && ! internetAvailable())
        {
            setState (Stage::starting, "Offline - Squelchmoth works fully offline. The Claude link will connect by itself when you are online.", {});
            for (int i = 0; i < 100 && ! threadShouldExit(); ++i) juce::Thread::sleep (100);
        }

        while (! threadShouldExit())
        {
            bool ok = false;
            if (provider == "ngrok")       ok = runNgrok();
            else if (provider == "pinggy") ok = runPinggy();
            else                           ok = runCloudflare (attempt);

            if (threadShouldExit() || userStopped) break;

            // the link process ended on its own: restart with a short backoff (claude.ai keeps retrying too)
            ++attempt;
            if (! internetAvailable())
            {
                // the network went away: wait for it instead of burning retries
                while (! threadShouldExit() && ! internetAvailable())
                {
                    setState (Stage::starting, "Offline - the Claude link will reconnect when you are back online.", {});
                    for (int i = 0; i < 100 && ! threadShouldExit(); ++i) juce::Thread::sleep (100);
                }
                attempt = 0;
                continue;
            }
            if (attempt > 6)
            {
                setState (Stage::failed, "The link keeps dropping. Try another provider in Setup > Preferences > Claude (ngrok works on strict networks).", {});
                return;
            }
            setState (Stage::starting, juce::String (ok ? "Link dropped - reconnecting" : "Link failed - retrying") + " (attempt " + juce::String (attempt + 1) + ")...", {});
            for (int i = 0; i < 30 * attempt && ! threadShouldExit(); ++i) juce::Thread::sleep (100);
        }
    }

    /** Pumps a child process, scanning output lines for the public URL and the "ready" marker. Returns when it exits. */
    bool pump (const juce::String& readyMarker, std::function<juce::String (const juce::String& line)> urlFromLine, int readyTimeoutMs, bool& registered)
    {
        juce::String partial, url;
        registered = false;
        auto started = juce::Time::getMillisecondCounter();
        char chunk[2048];
        bool reachabilityDone = false;

        while (! threadShouldExit() && process->isRunning())
        {
            auto n = process->readProcessOutput (chunk, sizeof (chunk) - 1);
            if (n <= 0) juce::Thread::sleep (80);
            else
            {
                chunk[n] = 0;
                partial += juce::String::fromUTF8 (chunk, n);
                while (partial.containsChar ('\n'))
                {
                    auto line = partial.upToFirstOccurrenceOf ("\n", false, false).trim();
                    partial = partial.fromFirstOccurrenceOf ("\n", false, false);
                    if (line.isEmpty()) continue;
                    diag (line.substring (0, 300));
                    if (line.contains ("INF") || line.contains ("ERR") || line.contains ("WRN") || line.contains ("cloudflare")) sawCloudflaredOutput = true;

                    if (url.isEmpty())
                    {
                        auto u = urlFromLine (line);
                        if (u.isNotEmpty())
                        {
                            url = u;
                            setState (Stage::linkCreated, "Link created - waiting for the network to confirm...", url);
                        }
                    }
                    if (! registered && url.isNotEmpty() && (readyMarker.isEmpty() || line.contains (readyMarker)))
                    {
                        registered = true;
                        setState (Stage::registered, "Connected - checking that the internet can reach it...", url);
                        recheckRequested = true;
                    }
                }
            }

            if (recheckRequested && url.isNotEmpty() && registered)
            {
                recheckRequested = false;
                reachabilityDone = true;
                testReachability (url);
            }

            if (! registered && readyTimeoutMs > 0 && (int) (juce::Time::getMillisecondCounter() - started) > readyTimeoutMs)
                return false;   // caller decides what to do (e.g. switch protocol)
        }
        juce::ignoreUnused (reachabilityDone);
        return registered;
    }

    bool runCloudflare (int attempt)
    {
        sawCloudflaredOutput = false;
        auto exe = findOrInstallCloudflared();
        if (! exe.existsAsFile())
        {
            setState (Stage::failed, "cloudflared is missing and could not be downloaded. Check your internet connection, or choose ngrok in Preferences > Claude.", {});
            return false;
        }

        // first try QUIC (fast), then HTTP/2 (works through more firewalls); both use port 7844
        for (int pass = (attempt > 0 ? 1 : 0); pass < 2 && ! threadShouldExit(); ++pass)
        {
            juce::StringArray args { exe.getFullPathName(), "tunnel", "--no-autoupdate", "--edge-ip-version", "4",
                                     "--protocol", pass == 0 ? "quic" : "http2", "--url", "http://127.0.0.1:" + juce::String (port) };
            process = std::make_unique<juce::ChildProcess>();
            if (! process->start (args))
            {
                setState (Stage::failed, "Could not start cloudflared (" + exe.getFullPathName() + ").", {});
                return false;
            }
            setState (Stage::starting, pass == 0 ? "Connecting to Cloudflare..." : "Connecting to Cloudflare over HTTP/2 (fallback)...", {});

            bool registered = false;
            auto exited = pump ("Registered tunnel connection",
                                [] (const juce::String& line) -> juce::String
                                {
                                    auto idx = line.indexOf ("https://");
                                    while (idx >= 0)
                                    {
                                        auto c = line.substring (idx).upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("|", false, false).trim();
                                        if (c.contains (".trycloudflare.com") && ! c.contains ("api.")) return c;
                                        idx = line.indexOf (idx + 8, "https://");
                                    }
                                    return {};
                                },
                                pass == 0 ? 30000 : 0, registered);

            if (registered && ! exited) { /* unreachable: pump returns after exit when registered */ }
            if (registered) return true;                 // process ran until it exited after registering
            if (threadShouldExit()) return true;
            // not registered within the timeout: kill and try the next protocol
            process->kill();
            process.reset();
            diag (pass == 0 ? "QUIC did not register in 30 s - switching to HTTP/2" : "HTTP/2 did not register either");
        }
        // Nothing registered. If we were running the copy bundled inside the .app and it produced no useful output
        // at all, treat the bundle as damaged: download a fresh pinned copy and try once more.
        if (! bundleDistrusted && bundledCloudflared().existsAsFile() && ! sawCloudflaredOutput)
        {
            bundleDistrusted = true;
            diag ("bundled cloudflared produced no output - fetching a fresh copy");
            setState (Stage::preparing, "Bundled tunnel helper did not start - downloading a fresh copy...", {});
            return runCloudflare (0);
        }
        setState (Stage::failed, "Cloudflare could not connect (port 7844 is probably blocked on this network). Switch to ngrok or Pinggy in Setup > Preferences > Claude - both use the normal web port 443.", {});
        return false;
    }

    bool runNgrok()
    {
        auto token = Prefs::ngrokToken().trim();
        if (token.isEmpty())
        {
            setState (Stage::failed, "ngrok needs a free authtoken: sign up at ngrok.com, copy the authtoken from the dashboard into Preferences > Claude, then press Connect again.", {});
            juce::Thread::sleep (500);
            signalThreadShouldExit();
            return false;
        }
        auto exe = findOrInstallNgrok();
        if (! exe.existsAsFile()) { setState (Stage::failed, "ngrok could not be downloaded. Check your internet connection.", {}); signalThreadShouldExit(); return false; }

        juce::StringArray args { exe.getFullPathName(), "http", "--authtoken", token, "--log", "stdout", "--log-format", "logfmt" };
        auto domain = Prefs::ngrokDomain().trim();
        if (domain.isNotEmpty()) { args.add ("--url"); args.add (domain); }
        args.add (juce::String (port));
        process = std::make_unique<juce::ChildProcess>();
        if (! process->start (args)) { setState (Stage::failed, "Could not start ngrok.", {}); return false; }
        setState (Stage::starting, "Connecting to ngrok...", {});

        bool registered = false;
        pump ("", [] (const juce::String& line) -> juce::String
        {
            if (! line.contains ("started tunnel") && ! line.contains ("url=")) return {};
            auto idx = line.indexOf ("url=https://");
            if (idx < 0) return {};
            return line.substring (idx + 4).upToFirstOccurrenceOf (" ", false, false).trim();
        }, 0, registered);
        if (! registered)
        {
            auto d = getDiagnostics();
            if (d.contains ("ERR_NGROK_108") || d.contains ("authentication failed") || d.contains ("ERR_NGROK_105"))
                setState (Stage::failed, "ngrok rejected the authtoken (or the account already has a tunnel open). Check the token in Preferences > Claude.", {});
        }
        return registered;
    }

    bool runPinggy()
    {
       #if JUCE_WINDOWS
        juce::String ssh = "ssh.exe";
        const char* nullDevice = "NUL";
       #else
        juce::String ssh = "/usr/bin/ssh";
        const char* nullDevice = "/dev/null";
       #endif
        juce::StringArray args { ssh, "-p", "443", "-R0:127.0.0.1:" + juce::String (port), "-o", "StrictHostKeyChecking=no", "-o", juce::String ("UserKnownHostsFile=") + nullDevice,
                                 "-o", "ServerAliveInterval=30", "-o", "ExitOnForwardFailure=yes", "-tt", "a.pinggy.io" };
        process = std::make_unique<juce::ChildProcess>();
        if (! process->start (args)) { setState (Stage::failed, "Could not start ssh for Pinggy.", {}); return false; }
        setState (Stage::starting, "Connecting to Pinggy over SSH (port 443)...", {});
        bool registered = false;
        pump ("", [] (const juce::String& line) -> juce::String
        {
            auto idx = line.indexOf ("https://");
            if (idx < 0) return {};
            auto c = line.substring (idx).upToFirstOccurrenceOf (" ", false, false).trim();
            return c.contains ("pinggy") ? c : juce::String();
        }, 0, registered);
        return registered;
    }

    /** Fetches our own /health through the public address to prove the path claude.ai will use. */
    void testReachability (const juce::String& url)
    {
        juce::URL u (url + healthPath);
        juce::String error;
        bool ok = false;
        for (int attempt = 0; attempt < 6 && ! ok && ! threadShouldExit(); ++attempt)
        {
            int status = 0;
            auto stream = u.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                   .withConnectionTimeoutMs (8000).withStatusCode (&status).withNumRedirectsToFollow (2));
            if (stream != nullptr)
            {
                auto body = stream->readEntireStreamAsString();
                diag ("[reachability] HTTP " + juce::String (status) + " " + body.substring (0, 120).replace ("\n", " "));
                if (status == 200 && body.contains ("squelchmoth")) ok = true;
                else error = "HTTP " + juce::String (status);
            }
            else { error = "no response"; diag ("[reachability] no response (attempt " + juce::String (attempt + 1) + ")"); }
            if (! ok) juce::Thread::sleep (2500);
        }
        if (ok) setState (Stage::reachable, "Connected", url);
        else setState (Stage::registered, "Connected (link up, but the self-check from this Mac got: " + error + " - claude.ai may still work; press Test again in a few seconds)", url);
    }
};
