# Squelchmoth — setup guide

Squelchmoth is a full digital audio workstation created and owned by Jamar Stewart. It runs on
macOS 11 Big Sur and newer (Intel and Apple Silicon) and on Windows 10/11, hosts your VST3 and
Audio Unit plugins, ships 200 built-in processors and instruments plus a beat maker, and has a
built-in control server so Claude can run the whole session.

## A. Install on your Mac (Big Sur or newer)

1. Double-click `Squelchmoth-macOS.zip`. A folder appears with `Squelchmoth.app` and `START HERE.txt`.
2. Drag `Squelchmoth.app` into your **Applications** folder.
3. In Applications, **right-click** (or Control-click) `Squelchmoth` and choose **Open**.
4. macOS says it cannot verify the developer. Click **Open**. You only do this once.
5. If macOS asks for microphone access, click **OK** (needed to record audio).
6. Squelchmoth opens with a session called *My First Session* and starts scanning your plugins in the
   background. The bottom status bar shows progress; when it says *scan finished*, your VST3 and AU
   plugins are in every **+ insert** menu.
7. Choose your audio interface: menu **Setup → Audio & MIDI Settings…**.
8. **Your copy is free:** choose **Help → Licences and Upgrades…**, paste your owner key (kept privately, never in the repo or any zip)
   and click **Activate**. Owner keys count as Squelchmoth Complete. (Customers start on the free Demo and buy
   keys from the Payhip store; see `docs/internal/PAYHIP-SETUP.md`.)

## B. Squelchmoth Chat (Claude or ChatGPT inside the app)

Click **CHAT** (or Cmd/Ctrl+Shift+C) and type. The **CLAUDE / CHATGPT** switch at the top of the chat picks who
runs the session; both get every Squelchmoth tool (full control) and list each step in the chat.

1. Click **CLAUDE** or **CHATGPT** in the switch.
2. Click the gear button. Paste that provider's API key (Anthropic key for Claude, OpenAI key for ChatGPT) and
   click **Verify**. The model menu fills with the models on your account. Each key is kept separately and stays on
   this computer.
3. Type what you want and press Enter.

You can switch in the middle of a chat: the next message goes to the other assistant with the whole conversation so
far. Claude uses the Anthropic Messages API (version 2026-10-21); ChatGPT uses the OpenAI Responses API with
function calling. API use is billed by each company to your own API account (a claude.ai or ChatGPT subscription
does not cover API use).

## B2. Connect Claude from claude.ai (about one minute)

1. Click **CLAUDE** at the top right of the window.
2. Click **CONNECT CLAUDE**. Wait until the second status line says **Connected** (5–20 seconds).
3. Click **COPY LINK**.
4. Click **OPEN CLAUDE CONNECTORS**. Your browser opens https://claude.ai/customize/connectors.
5. Click **Add custom connector**. Name: `Squelchmoth`. Paste the link into the URL box.
   If the dialog asks about authentication, choose **No sign-in**. Click **Add**.
6. Open any chat, click **+** → **Connectors**, switch **Squelchmoth** on, and ask for anything:
   *"Make a 92 BPM lo-fi beat with drums, keys and bass, send the keys to a reverb bus, and bounce it at −14 LUFS."*

The link changes every time you press **CONNECT CLAUDE**. After restarting Squelchmoth, edit the
Squelchmoth connector on claude.ai and paste the new link, or tick **Connect automatically** in the
CLAUDE panel so the link is ready when the app opens.

## B2b. Make a beat like a reference (1.15)

1. Drop an instrumental (WAV, AIFF, FLAC, MP3 or OGG) on Squelchmoth Chat. The chat starts listening right away and
   writes the request for you; or drag it onto an audio track and ask for "a beat like the instrumental on track 3".
2. Press Return. Claude or ChatGPT calls the Reference Listener (`analyze_reference`) first: tempo and half-time feel,
   key, swing, sections with bar numbers, the instruments in each section, the chord functions, the kick / snare / hat
   grid, hat rolls, the 808 line and glides, and the mix balance.
3. It builds original parts in that feel (drums, chords, 808, melody, arrangement, mix), bounces, compares the result
   with the reference (`analyze_reference` action compare) and fixes the biggest gaps.
4. Nothing of the reference is copied or stored: only measurements.

## B3. Connect ChatGPT from chatgpt.com (same link, same full control)

1. Do steps 1-3 of B2 (connect, copy the link).
2. In the CLAUDE panel click **OPEN CHATGPT GUIDE** (OpenAI's page on adding your own MCP server; it needs
   Developer mode on a paid ChatGPT plan, on the web).
3. Following that page, turn on Developer mode and create a new app: name `Squelchmoth`, MCP server URL = the link,
   no authentication. Save it.
4. In a new ChatGPT chat open **+**, choose Developer mode, select Squelchmoth and ask for anything. ChatGPT asks you
   to approve actions that change the session before it runs them.

If the status never reaches *Connected*, your network is blocking Cloudflare tunnels (outbound port 7844).
Try another network or turn off a VPN. Claude Desktop and Claude Code on the same Mac can use the
*Local link* shown in the panel without any tunnel.

## C. Make a beat without Claude

1. Click **+ Track → Drum track (beat maker + built-in kit)**. The Beat Maker window opens.
2. Click cells to add hits, drag to paint, right-click for an accent, shift-click to clear a row.
3. Press **Space** to play. Double-click the orange *Beat* clip any time to reopen the grid.
4. Swap a pad for your own sample: ask Claude (`load_drum_sound`) or right-click the Sampler insert.

## D. What Claude can do (47 commands)

Session: get_session, new_session, open_session, save_session, list_sessions, undo
Transport: transport (play/stop/record/locate), set_loop, set_tempo, set_click
Tracks: add_track, delete_track, update_track (name, volume, pan, mute, solo, arm, monitor, colour, input, output, order), list_inputs
Clips: list_clips, import_audio, edit_clip, split_clip, delete_clip, duplicate_clip
MIDI: create_midi_clip, add_midi_notes, get_midi_notes, quantize_midi, transpose_midi
Beats: add_drum_track, set_beat, get_beat, load_drum_sound
Plugins: list_plugins, scan_plugins, insert_plugin, remove_plugin, bypass_plugin, move_plugin,
         get_plugin_parameters, set_plugin_parameters, plugin_presets, open_plugin_window
Mix: add_send, write_automation, clear_automation, get_meters, analyze_mix, bounce
View: screenshot, show_view

## E. Keyboard

Space play/stop · Cmd+Space record · Return to start · L loop · C click · B split · Delete clip ·
Cmd+D duplicate · Cmd+Z / Cmd+Shift+Z undo/redo · Cmd+S save · Cmd+= Edit/Mix ·
Cmd-scroll zoom · drag in the ruler sets the loop · drop audio/MIDI files on lanes ·
double-click a MIDI clip = piano roll · double-click a Beat clip = Beat Maker.

## F. Windows and future builds (GitHub, free)

The Mac app in this delivery was cross-compiled and verified. For the Windows build, an Apple-Silicon
refresh, or any future version, the included GitHub workflow builds everything automatically:

1. Sign in at https://github.com (free account is fine).
2. Go to https://github.com/new, name it `squelchmoth`, choose **Private**, click **Create repository**.
3. Click **uploading an existing file**, unzip `squelchmoth-source.zip`, select everything inside the
   `squelchmoth` folder (press Cmd+Shift+. in Finder to show the hidden `.github` folder), drag it onto the
   page and click **Commit changes**.
4. Open the **Actions** tab (enable workflows if asked). The build takes 30–45 minutes.
5. When it is green, go to **Releases → Squelchmoth (latest build)** and download `Squelchmoth.dmg`
   (Mac, universal) and `Squelchmoth-Windows.zip`.

## G. Files and folders

- Sessions: `~/Documents/Squelchmoth Sessions/` (`.squelch` files; autosaved every minute)
- Bounces: `~/Documents/Squelchmoth Sessions/Bounces/`
- Drum kit: `~/Documents/Squelchmoth Sessions/Kits/Squelchmoth Kit/`
- Saved plugin presets: `~/Documents/Squelchmoth Sessions/Presets/<plugin>/`
- Settings and the Claude secret: `~/Library/Application Support/Squelchmoth/`

See `PLUGINS.md` for the full plugin list and `BUSINESS.md` for licensing and how to charge for memberships.
