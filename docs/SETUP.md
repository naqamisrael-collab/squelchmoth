# Squelchmoth — setup guide

Squelchmoth is a full digital audio workstation created and owned by Jamar Stewart. It runs on
macOS 11 Big Sur and newer (Intel and Apple Silicon) and on Windows 10/11, hosts your VST3 and
Audio Unit plugins, ships the 69-plugin Squelchmoth Signature bundle (twelve instruments) plus 186 more
effects and 17 drum sound kits, puts FL Studio's Channel Rack, Beat page and instruments next to the
Pro Tools edit modes, tools, windows and transport and Studio One's page layouts (Edit + Mix at once),
and has a built-in control server so Claude can run the whole session.

## A. Install on your Mac (Big Sur or newer)

1. Double-click `Squelchmoth-Mac-Intel.zip` (Intel Mac) or `Squelchmoth-Mac-AppleSilicon.zip` (M1 and newer).
   A folder appears with **Squelchmoth.app**, **Install Squelchmoth**, **Uninstall Squelchmoth**, `START HERE.txt` and the guides.
2. **Right-click** (or Control-click) **Install Squelchmoth** and choose **Open**, then **Open** again. It copies
   Squelchmoth (and the uninstaller) into **Applications**, clears the download flag so the app opens normally, and
   offers to launch it. macOS asks this once because the app is not from the App Store (if the dialog only offers
   *OK*, click it, open **System Preferences → Security & Privacy → General** and click **Open Anyway**).
   Prefer to do it by hand? Drag `Squelchmoth.app` into Applications, then right-click it → **Open** the first time.
3. Open Squelchmoth from Applications or Launchpad.
4. An older version is replaced; your sessions, settings and licence are kept.
5. If macOS asks for microphone access, click **OK** (needed to record audio).

**Offline.** Everything except the Claude link works with no internet at all: recording, editing, mixing,
plugins, kits, the beat maker, mastering and exports, and your licence keys. While you are offline
the Claude link waits quietly (status bar: *Offline*) and connects by itself when the internet comes back.

6. The **Quick Start** window opens. Choose *New Session*, *New from Template* or *Open*.
   Squelchmoth finds your VST3 and Audio Unit plugins **automatically** in the background on every launch
   (only new files are scanned). Progress is in the status bar and in **Setup → Plugins Window**.
7. Choose your audio interface: **Setup → Playback Engine…**.
8. **Editions:** without a key Squelchmoth runs as the free **Demo** (no time limit): 16 tracks, the basic
   instruments and effects, kits 1–5, save and bounce your mix. To unlock what you bought, choose
   **Help → Licences and Upgrades…**, paste the key from your purchase email (it starts with `SQM2-`) and click
   **Activate**. Keys stack: enter one per product (Squelchmoth, Complete or an add-on pack). Plug-ins you don't
   own yet load as *Locked* panels that keep their settings; after unlocking, click **Reopen session to unlock**.
   Buy at squelchmoth.com, or from the Buy buttons in the same window.

## B. Connect Claude (about one minute)

1. Click **CLAUDE** at the top right. The panel shows five status lights.
2. Click **CONNECT CLAUDE**. The lights go green one by one: control server → link tool →
   public link created → *verified: the internet can reach Squelchmoth* (5–20 seconds).
3. Click **COPY LINK**.
4. Click **OPEN CLAUDE CONNECTORS**. Your browser opens **claude.ai → Settings → Connectors**.
5. Click **Add custom connector**. Name: `Squelchmoth`. Paste the link into the URL field.
   Leave authentication / OAuth empty (no sign-in). Click **Add**.
6. Open any chat, click the **+** (tools) button → **Connectors**, switch **Squelchmoth** on and ask:
   *"Use Squelchmoth: make a 92 BPM lo-fi beat with the Velvet Lo-Fi kit, keys and bass, send the keys
   to a Nebula Verb bus and bounce it at −14 LUFS."*  The fifth light turns green the first time Claude calls in.

**Link providers.** The default link is a Cloudflare quick tunnel; the tunnel program is now **bundled
inside the app** (nothing to download) and it falls back from QUIC to HTTP/2 automatically. If the
fourth light stays amber/red, your network blocks Cloudflare's port 7844. Open the **Link provider**
menu in the CLAUDE panel (or **Setup → Preferences → Claude**) and choose:

- **ngrok** — free account at ngrok.com, paste the authtoken when asked. Uses the normal web port (443),
  works on strict networks, and can keep a **fixed address** (claim the free static domain in the ngrok
  dashboard and enter it in Preferences → Claude), so you never re-paste the link on claude.ai.
- **Pinggy** — no account, runs over SSH on port 443; the link changes every 60 minutes.

**Self-test and diagnostics.** **Claude → Run connection self-test** checks every stage on your Mac.
**Claude → Copy diagnostics for support** copies a report you can paste into your Claude chat — it will
tell you exactly what to fix. Custom connectors require a Claude Pro, Max, Team or Enterprise plan.

With Cloudflare or Pinggy the link address changes each time you connect, so after restarting Squelchmoth
edit the connector on claude.ai and paste the new link, or tick **Connect automatically at launch** (default on)
so it is ready when the app opens. Claude Desktop / Claude Code on the same Mac can use the **Local link**
shown in the panel with no internet link at all.

## C. The Edit window (Pro Tools layout)

Toolbar row 1: **edit modes** (Shuffle F1, Slip F2, Spot F3, Grid F4), **tools** (Zoomer F5, Trim F6, Selector F7,
Grabber F8, Scrubber F9, Pencil F10, Smart), zoom buttons, the **main counter** (with the sub counter under it; the
**...** buttons choose Bars|Beats, Min:Secs, Timecode or Samples for each), and the **Edit selection** start / end /
length fields (type any time format into them).
Row 2: **grid value**, **REL** (Relative grid), **+GRID** (combine the grid with Shuffle or Slip), **nudge value**,
**Tab to Transient**, **Link Timeline & Edit**, **INS FOLLOW** (Insertion Follows Playback) and **SCROLL** (No / Page /
Continuous scrolling).

**Edit modes, exactly like Pro Tools:**
- **Shuffle** - clips are tiles: drag one and the others close up behind it and make room where you drop it; trimming
  or deleting ripples everything after it. **Cmd-click Shuffle = Shuffle Lock** (F1 can't switch to it by accident).
- **Slip** - free placement; a clip dropped over another covers that part of it (one clip sounds at a time).
  Hold Cmd while dragging to snap to the grid.
- **Spot** - clicking a clip (or dropping a file) opens the Spot dialog: Start / End / Duration in any time format,
  **Use Original Time Stamp** (from the recording) and **Use User Time Stamp** (Clip menu → Time Stamp Selected).
- **Grid** - **Absolute**: the clip start lands on a grid line; **Relative** (REL): it moves in whole grid steps and
  keeps its offset. The grid follows every tempo and meter change. Cmd suspends it while dragging.
- **Shift-click Grid** while in Shuffle or Slip combines them (the button lights as +GRID).

**Rulers** (click the ruler names on the left to show or hide each one): **Bars|Beats**, **Min:Secs**, **Timecode**
(23.976, 24, 25, 29.97, 29.97 drop-frame, 30 fps - set in Session Setup with the start time, e.g. 01:00:00:00),
**Samples**, **Tempo** (drag a tempo up or down to change it, double-click to add one, right-click to change or delete),
**Meter** (double-click to add a meter change) and **Markers** (double-click to add a memory location, drag markers,
right-click to edit or delete). Drag in the Bars ruler to set the loop / timeline selection.

**Transport like Pro Tools:** Stop returns to where playback started (turn on **INS FOLLOW** to stay where you stopped);
playing an Edit selection stops at its end; the playhead moves at your screen's refresh rate.

Smart Tool: top half of a clip = Selector, bottom half = Grabber, clip edges = Trim, top corners = fade in / out,
**bottom corner where two clips meet = crossfade** (drag sideways: an equal-power crossfade that uses the hidden audio
past the clip edges), the amber line in the clip = **clip gain** (drag it). A selection across two touching clips plus
**Cmd+F** also makes a crossfade. Shift-click adds clips to the selection, Cmd-click toggles, Opt-drag copies.

**Track heights:** Micro, Mini, Small, Medium, Large, Jumbo, Extreme or Fit to Window (right-click a track name →
Track Height, or View → Track Height for all). **Ctrl+Up / Ctrl+Down** step the selected tracks bigger / smaller.
Heights are saved with the session. **View → Waveform View**: Peak, Rectified or Power (log, quiet detail visible).

Track headers: name, **P** playlists/takes, **EL** Elastic Audio, automation mode (off/read/touch/latch/write),
R I S M buttons, **Ø** (polarity / phase invert - for two mics on one source), the **Track View** selector, a **volume fader** (drag; double-click = 0 dB; click the number to type a
value; Cmd-drag for fine moves) and a **pan** button. Folder and VCA tracks show their children indented; click the arrow
to collapse them. The **I** (input monitor) button is per track; MIDI Thru plays the first selected instrument track.

**Volume and pan in the I/O view.** With View → Edit Window Views → I/O on, each track shows its input, output, a
**volume indicator** and **pan indicator(s)**: click one for a pop-up fader or pan slider (two for a stereo track),
Opt-click to reset. The small fader button opens the track's **Output window** - fader, meter, pan, mute, solo, record,
input monitor, automation mode and I/O - and the menu at its top switches it to any track, like Pro Tools.

**Track View** (the *waveform* button on each track, or the automation button's menu): show **volume**, **pan** (left and
right on stereo tracks), **send levels** or **any plug-in parameter** as an automation line on the track and draw it with
the Pencil (F10). "Clear automation shown in this lane" is in the automation menu.

**Edit Window Views** (View → Edit Window Views, like Pro Tools): **I/O** (input and output selectors),
**Inserts A-E**, **Inserts F-J**, **Sends A-E**, **Sends F-J**, **Comments**, All / None. The columns sit between
the track names and the clips; click an empty slot to add a plugin or send.

**Right-click a track name** for the Pro Tools track menu: Rename, Hide, Hide and Make Inactive, Make Active /
Inactive, Duplicate..., Split into Mono, Freeze, Commit..., Bounce..., Delete..., Input / Output, Insert, Record
Safe, Solo Safe, Timebase (Samples / Ticks), Comments, Color, Groups, Folders / VCA (Move to New Routing or Basic
Folder), Track Height and Track Presets. Several selected tracks change together.

## C1. Tracks, I/O and the Master Fader

* **Track → New... (Cmd+Shift+N)** — also the **+ TRACK** button in the Edit and Mix windows, or right-click a track
  name → **New...** — opens the **New Tracks** window. Each row reads like Pro Tools: *Create [2] new [Stereo]
  [Audio Track] in [Samples] named [Vox]*. Types: **Audio Track, Aux Input, Master Fader, VCA Master, MIDI Track,
  Instrument Track, Basic Folder, Routing Folder** and every saved **Track Preset**. **Mono or Stereo** for Audio,
  Aux Input, Instrument, Master Fader and Routing Folder tracks (mono tracks are truly mono: a stereo source is
  summed to one channel before the inserts). **+** adds a row below, **-** removes it, drag the handle on the left
  to re-order. Keys: Cmd+Up/Down type, Cmd+Left/Right mono/stereo, Cmd+Opt+Up/Down timebase, Cmd+N add a row,
  Return create, Esc cancel. *Insert after last selected track* places them like Pro Tools, names continue
  Pro Tools-style (Audio 5, Audio 6...) and the window remembers the rows you used last time.
* **Quick New** (right-click **+ TRACK**, right-click empty space in the Edit or Mix window, or Track → Quick New):
  one click for Mono / Stereo Audio, Mono / Stereo Aux Input, Mono / Stereo Instrument (or any instrument plug-in),
  MIDI, Drum Track, Master Fader, VCA Master and Routing / Basic Folder.
* **Inputs and outputs are named like Pro Tools**: In 1, In 2... (mono), In 1-2, In 3-4... (stereo), Out 1-2,
  Out 3-4..., and busses by their names. New tracks take the next free input automatically.
* **Setup → I/O Setup** has **Input, Output, Bus and MIDI** tabs: click a name to rename it (*Vocal Mic*, *Kick In*),
  switch any path between Mono and Stereo, add / rename / delete busses (shown with the aux track each one feeds),
  rename or disable MIDI ports. **Default names**, **All mono** and **All stereo** reset the layout.
* **Input selector** (I/O column or Mix strip): *no input*, **interface** (every mono path and stereo pair),
  **bus**, and for MIDI / Instrument tracks the **MIDI** ports (All or one device). **Output selector**: *no
  output*, **interface**, **bus**, **track** (straight into an aux) or **new track...** to make a new aux
  with its bus in one step. Squelchmoth refuses routings that would feed back.
* **Master Fader**: Track → **New Master Fader** (or the New Tracks dialog). It appears last in the Edit window and
  at the right of the Mix window, can be renamed, has inserts A-J (dither last), volume automation, comments and
  its own output path. Track → Delete Master Fader resets it to 0 dB with no inserts.
* **Track menu**: New..., Group..., Duplicate... (playlists, automation, inserts, sends, group, copies), Split into
  Mono, Make Active / Inactive, Delete..., Routing / Basic Folder, VCA, Input Only / Auto Input Monitoring, Record
  Safe, Solo Safe, Freeze, Commit..., Bounce..., Clear All Clip Indicators, Timebase, Hide, Show All Tracks,
  Scroll to Track..., Automation and Elastic Audio, Save Track Preset.


## C2. The Mix window (Pro Tools console)

Every strip reads top to bottom like Pro Tools: colour bar, name, automation mode, **Inserts A-E**
(click the small **A-J** box beside INSERTS for ten - Inserts F-J), **Sends A-E** (the **A-J** box beside SENDS for
Sends F-J; View → Mix Window Views has the same switches). With every slot showing the Mix window scrolls up
and down so the faders keep their full length. I/O, pan, the long-throw fader with its
dB scale and peak-hold meter, the level and peak readouts, R / S / M.

* **Empty insert slot**: click it to pick a plugin (Squelchmoth Signature, your VST3/AU, AirWindows).
* **Filled insert slot**: click the name to open the plugin; click the little arrow (or right-click) for
  **No Insert (remove)**, Bypass, Replace with..., Move up / down, Copy insert, Paste insert, Save as preset.
  Double-click a slot to bypass it.
* **Send slot**: click to choose a bus (or make a new aux bus); drag the green bar for the send level
  (double-click = 0 dB); the arrow / right-click gives **No Send**, Mute, Pre-fader / Post-fader,
  Invert polarity, Change bus, Set level.
* **Pan**: drag the knob, double-click to centre. Right-click it for the session **Pan Depth**
  (-2.5 / -3 / -4.5 / -6 dB, the same choices as Pro Tools' Session Setup) and **Stereo pan**, which gives a
  stereo track Pro Tools' two knobs (L and R, link / inverse link, hard L-R, mono). Pan Depth is also in
  Window → Session Setup.
* The master strip has its own inserts (put Dither Dust last) and fader; the mix starts at 0 dB like Pro Tools.

## C2b. Plug-in menus and your plug-ins

Every plug-in menu (insert slots, AudioSuite, the Mastering racks, new instrument tracks) is organised like Pro Tools:
**EQ, Dynamics, Pitch Shift, Reverb, Delay, Modulation, Harmonic, Dither, Sound Field, Noise Reduction, Channel Strip,
Instrument, Effect, Other**, each with Squelchmoth's own plug-ins first and yours below, plus the AirWindows collection.
Categories come from the plug-in itself, then a catalogue of well-known products (Waves, FabFilter, UAD,
iZotope, Plugin Alliance, Slate, Valhalla, Native Instruments...), then the name.

* **Waves and other shell plug-ins** show each plug-in once - not separate Mono, Stereo and Mono/Stereo copies. The right
  layout is chosen for the track (stereo for stereo tracks, mono-to-stereo for reverbs and delays on mono tracks).
* A plug-in installed as **VST3 and Audio Unit** is listed once (VST3 by default).
* **Search plug-ins...** at the top of every menu: type part of a name, maker or category and press Return.
* **Setup → Preferences → Plugins**: organise menus by Category, Manufacturer, Category and Manufacturer or a flat list;
  show VST3, Audio Unit or both.

**Plug-ins stay installed.** Squelchmoth finds new plug-ins at launch in the background and keeps the list: it is saved
after every change, with a backup that is restored at launch. Quitting while a scan is running no longer hides plug-ins
(the scan simply continues next time). If a session uses a plug-in that is not on the list yet, Squelchmoth scans and
loads it by itself. Setup → Rescan All Plugins starts from scratch if you ever need to.

## C3. Plugin windows

Every Signature plugin has its own face: artwork, a live display (EQ curve with draggable band dots,
compressor transfer curve with gain-reduction meter, tape reels, reverb cloud, delay taps, LFO scope,
oscillators and envelopes...), knobs in the plugin's style, and a **Presets** menu with a *Basic* and an
*Advanced* bank (12-16 per plugin), *Your presets* (saved from the same menu) and *Reset to default*.
**A/B** stores the current settings and lets you flip between two versions; **BYPASS** is the plugin bypass.
Knobs: drag up/down, Cmd-drag for fine control, double-click to reset, right-click to type a value.
Drop a WAV on **Cavern IR** to load an impulse response, or on **Chrysalis Sampler** to add a zone.

## D. Windows (Window menu)

Transport (Cmd+1) · Big Counter (Cmd+3) · Memory Locations (Cmd+5) · Clip List · Groups (Cmd+G) ·
Undo History · Color Palette (Cmd+Shift+K) · Session Setup (Cmd+2) · I/O Setup · Playback Engine ·
Plugins · Preferences (Cmd+,) · MIDI Editor · Beat Maker · Claude Panel · Quick Start · **Mix Snapshots (A-H)**.

**Mix Snapshots** store a whole mix - every fader, pan, mute, solo, polarity, send level, insert bypass and plug-in
setting, the Master too - in eight slots saved with the session. Click a letter to recall it (an empty one stores the
current mix), **Store** to overwrite, **Compare** flips between the last two you recalled, **Undo Recall** goes back,
and **Glide** moves everything smoothly over 0.5-8 seconds.

**Bit depth and sample rate - where to find them:**
1. **New session:** File → New Session... shows **Sample rate** and **Bit depth** (16-bit, 24-bit, 32-bit float) under the name and tempo. Pick them, then click **Create**.
2. **Existing session:** Setup → Session Setup (Cmd+2). Change **SAMPLE RATE** or **RECORD BIT DEPTH** in the row of drop-downs. Recordings made after the change use it.
3. **Bounces and stems:** File → Bounce Mix... or File → Export Stems... has its own **BIT DEPTH** (16 / 24 / 32-bit float) and **SAMPLE RATE**.
4. The status bar at the bottom always shows the current setting, for example "48.0 kHz / 32-bit float / 256 samples".

**Session Setup** (Cmd+2): the session **sample rate** (44.1-192 kHz - the interface is switched to it, and recordings
and bounces use it), **record bit depth** (32-bit float by default, which can't clip), **timecode rate** and **start
timecode**, tempo and meter at bar 1, and **pan depth** (applies to mono and dual panners; stereo faders, auxes,
folders and the Master are unity at centre, so a stereo file plays bit-exact).

**File → Bounce Mix...** (Cmd+Opt+B): name, WAV / AIFF / FLAC, 16 / 24-bit or 32-bit float, the session rate or any
rate to 192 kHz, whole session or the Edit selection, a tail for reverbs, loudness normalising (-14 LUFS streaming,
-16 Apple Music, -9 club, -23 broadcast, true peak held under -1 dBTP), dither and Master inserts on / off.
**File → Export Stems...** writes every track (or the selected tracks, or each routing folder) as its own file in one
offline pass. All stems start at the same point and have the same length, so they line up in any DAW and add up
exactly to the mix. Name them with a template: `{n} {track}`, `{session} - {track} {bpm}bpm`, `{date}`, `{sr}`.
"Include send effects" prints each track's reverb / delay sends into its stem.

**Insert chains:** click any insert slot → **Insert chain** → **Save insert chain...** saves every plug-in on that
track (with settings and bypass, in order). Load it on any track after the existing inserts or replacing them.

The **Transport window** has everything the Pro Tools one has: Online (greyed, needs timecode hardware),
RTZ, rewind, stop, play, fast-forward, end, record, Loop Playback, Pre-roll / Post-roll with lengths,
record mode (Normal / Loop Record / QuickPunch), Wait for Note, MIDI Merge, Click, Count-off (1 or 2 bars),
Conductor, tempo and meter.

## D2. Performance: Playback Engine, System Usage and crash protection

**Setup → Playback Engine** has your audio interface on the left (device, channels, test tone, sample rate and the
**H/W Buffer Size**) and Pro Tools' **Host Engine** on the right:

* **Host Processors** — how many CPU cores share the audio work (all of them by default).
* **CPU Usage Limit** — the most processing time audio may take. Above it the engine skips a block instead of
  freezing the Mac, so the app always stays responsive.
* **Ignore Errors During Playback/Record** — keep playing through overloads (you may hear clicks).
* **Dynamic Plug-in Processing** — plug-ins only use CPU while they have audio: an effect fed silence pauses once its
  own tail has died away and wakes on the next sound. Bypassed plug-ins never use CPU.
* **Optimize Performance at Low Buffer Sizes** — steadier low-latency recording; turn off to save heat on a laptop.
* **Overload Protection** (Squelchmoth) — when dropouts repeat during playback, the H/W buffer goes up one step
  instead of stopping. It never changes anything while you record (you are told afterwards). With protection and
  Ignore Errors both off, playback stops with an explanation, like Pro Tools.
* **Disk Playback Cache Size** — how much of each audio file is read ahead into memory (Normal to Maximum).

**Window → System Usage** (or click the CPU % at the top right) shows live CPU and peak against the limit, overloads,
interface dropouts, memory, the disk cache and the **heaviest plug-ins** with **Freeze track** and **Bypass** buttons.
Rules of thumb: 64–128 samples to record and play instruments, 512–1024 to mix big sessions, freeze tracks whose
plug-ins you have finished with, and make unused tracks inactive (Track → Make Inactive).

**Crash protection.** If Squelchmoth ever closes unexpectedly, the next launch tells you what happened, reopens your
session (auto-saved every minute) and switches off the third-party plug-in that was running when it crashed — its
settings stay in the session. **Setup → Plug-ins Switched Off** turns it back on (after you update it). A crash while
a session was opening reopens it in **Safe Mode** (third-party plug-ins not loaded). Hold **Shift** while Squelchmoth
starts to choose Safe Mode yourself; **Setup → Leave Safe Mode** loads the plug-ins again.
**Setup → Show Last Crash Report** finds the report to send to a plug-in's maker.

There is no separate driver to install on a Mac: Squelchmoth talks to Core Audio (macOS's own audio driver system)
directly, the same way Pro Tools, Logic and Studio One do. Use your interface maker's macOS driver if they provide
one, or an Aggregate Device from Audio MIDI Setup to combine interfaces.

## E. Make it yours

**Setup → Preferences → Display**: theme presets (Moth, Carbon, Daylight, Midnight Blue, Studio Grey) and a
swatch for every colour — **Edit window background, Mix window background**, panels, text, accents,
waveform and grid colours — plus the 16-colour **track palette** and text size. Changes apply instantly.
**Window → Color Palette** recolours the selected tracks, clips or markers with one click.
The other Preferences tabs cover Operation (play from selection, pre/post-roll, count-off, autosave),
Editing (fade lengths and shapes, Strip Silence defaults, clip names/gain line, default track height),
Mixing (new-track and send levels, meters, groups, narrow strips), MIDI, Plugins (extra folders) and Claude.

## F. Make a beat (the Beat Lab)

The Beat Maker (1.7) is a drum-machine step grid on one track: the drum track stays a normal instrument track (inserts,
sends, I/O, automation, playlists all work), and its clip opens the Beat Maker. The full FL Studio Channel Rack - one
channel per sound or instrument, patterns, the Graph Editor, Channel Settings - is chapter F5, and the Beat page is F4.

**Start in one click**
1. **Track > New > Drum Track** (or right-click empty space > Drum Track). Pick a kit at the top right - 17 kits:
   Mothwing 808, Hive 909, Chrome Trap, Velvet Lo-Fi, Dust Acoustic, Neon Trap, Drill Smoke, Afro Sun, Amapiano Log,
   Dembow Heat, Jersey Bounce, Phonk Cowbell, Boom Bap Crates, House Silk, Techno Steel, R&B Velvet, Rock Room.
2. **Beat Starter**: choose a genre (Trap, Drill, Boom Bap, Lo-Fi, R&B, Afrobeats, Amapiano, Reggaeton, Jersey Club,
   House, Techno, Phonk, Pop, Drum & Bass, Dancehall, Rock, Funk / Neo-Soul) and click **Roll beat**. Tick **Set tempo**
   to get the genre's usual BPM too. Roll again for a new variation; press **L** on a row to lock it first.
3. Press **Space**.

**Program it**
- Click a step to add it, drag to paint, right-click or right-drag to erase, **Alt-click** for an accent.
- **Double-click a step for a roll** (2, 3, 4, 6, 8 hits - trap and drill hats).
- Click a sound's name to hear it; **L / M / S** lock, mute and solo; each sound has its own level and pan.
- **Lanes** under the grid edit the selected sound step by step: **Velocity**, **Gate** (length), **Chance**
  (probability), **Shift** (micro-timing, up to half a step early or late) and **Repeat** (rolls). Drag to draw,
  right-drag for a straight line, Alt-click resets a step.
- Right-click a sound's name: Fill every 2 / 3 / 4 / 8 / 16 steps, offbeats, **Euclidean** rhythms, rotate,
  dice, humanise, **row length** (polyrhythm - e.g. a 3-step perc over a 16-step beat), swing amount, copy / paste, clear,
  or re-roll just that row.
- **Steps** 4 to 128, **resolution** 1/8, 1/16, 1/32 or triplets, **Swing** 0-100% for the whole pattern.

**Patterns and song**
- Patterns **A-H** per clip (right-click a letter to copy, paste, clear or add it to the chain). Keys A-H switch too.
- **Chain**: type the play order, e.g. `A A B A`, and it repeats for the whole clip (resize the clip in the Edit
  window and it keeps repeating). **Loop this pattern** keeps looping the one you're editing while you work.
- **Copy / Paste / Double / Clear** work on the pattern you're editing; Cmd+Z undoes everything.
- **To Piano Roll** turns the beat into an ordinary MIDI clip and opens it in the MIDI Editor.

## F1. MIDI Editor (piano roll)

Double-click a MIDI clip (or Event > MIDI Editor).
- Tools: **Draw (P)**, **Select (E)**, **Erase (D)**, **Slice (C)**. Right-click deletes in any tool, Alt-drag copies,
  Cmd+D duplicates, arrows move (Shift = octave), Q quantises, Cmd+wheel zooms.
- **Key**: pick root and scale (16 scales). Rows in the key are highlighted; **Scale snap** keeps every drawn or moved
  note in key and makes the arrow keys move by scale steps.
- **Chord stamp**: choose one of 21 chord types and an inversion - every click draws the whole chord.
- **Chords** menu: chord progressions by mood (dark trap, drill, sad piano, cinematic, pop, R&B, neo-soul, lo-fi jazz,
  afro / amapiano, gospel, reggaeton, house, phonk) in the key you picked, voice-led, with an optional bass / 808 line.
- **Tools** menu (selection, or every note): quantise (strength and swing), humanise, strum, arpeggiate (up, down,
  up-down, random, 1/8 to 1/32, triplets, two octaves), flam, legato, **808 slides**, chop, snap / transpose in key,
  velocity, flip, and the **riff starter** (a random melody in key).
- **Ghost notes** show the other MIDI tracks' notes hollow. The **velocity lane** at the bottom draws velocities.

## F1b. Moth 808 and Blossom (Squelchmoth instruments)

- **Moth 808** (Track > New > 808 Track): punch (pitch drop), click, harmonics, sub octave, drive (tube, tape, hard,
  fold - 4x oversampled), tone, decay and release. **Glide**: *Slide* glides when notes overlap (like FL Studio slide
  notes - use Tools > 808 slides in the MIDI Editor), *Always* glides between every note, *Off*. Mono or poly, pitch-bend
  range up to 24 semitones. 12 presets.
- **Blossom** (Track > New > Strings Track): physical-model strings. Each voice is two coupled waveguide strings with
  a pluck, hammer or bow exciter; drag the **Material** pad for nylon, gut, steel, glass or wood; pick a body (guitar,
  violin, cello, koto, kalimba, piano, glass); sympathetic strings, vibrato, chorus, delay and reverb. 32 voices,
  17 presets from nylon guitar and koto to violin, cello, kalimba, glass bells and ambient bowed pads.
- Both are in tune to within a few cents, use no CPU while silent and are fully automatable.

## F2. Master an album (Window → Mastering, Cmd+Shift+M)

The Mastering window is Squelchmoth's version of the Studio One / Fender Studio Pro **Project page**:

1. **New** names the album. **Add Files...** adds WAV/AIFF/FLAC/MP3/OGG tracks; **Add Current Session**
   bounces the session open in the Edit window and adds it (later **Update from Session** re-bounces it).
   You can also drop files or .squelch sessions on the window.
2. The **Track column** lists the album in order (Up / Down to re-order). Double-click a track or press
   **Track Info...** for title, artist, songwriter, composer, **ISRC** (enter the first one and the rest can
   be numbered automatically), the **pause** before the track (2-10 s before track 1, as the Red Book
   CD standard requires), fade in / out and the CD flags. **Album Info...** holds the disc title, artist,
   genre, year, **UPC/EAN**, label, catalog, copyright and artwork.
3. The **Track lane** shows the whole album with its pauses and blue track markers; click to locate,
   double-click to play from there, Space to play / stop.
4. **Track Device Rack**: inserts and a fader for the selected track (balance the songs against each
   other here). **Master Device Rack**: inserts and the master fader for the whole album
   ("Activate all / bypass all" A/Bs the processing).
5. **Detect Loudness** measures the selected track (EBU R128 integrated LUFS, loudness range, true peak,
   RMS - pre-FX and post-FX) and shows it in the list.
6. The meters at the bottom run all the time: **Spectrum** (octave, 1/3 octave, FFT, FFT curve, freeze),
   **Level / Loudness** (Peak/RMS, EBU R128 with momentary, short-term, integrated, LRA and true peak,
   K-20 / K-14 / K-12 scales, clip indicator, reset) and the **Phase** meter (goniometer + correlation).
7. Publish: **Digital Release...** writes one tagged WAV / AIFF / FLAC / OGG per track and can match every
   track (or the album as a whole) to a streaming service's loudness - Spotify, Apple Music, YouTube,
   Amazon, Tidal, Deezer, SoundCloud, podcast, broadcast, CD-loud or custom - with the max true peak
   always winning. **Image...** writes one continuous 16-bit / 44.1 kHz WAV plus a CUE sheet.
   **DDP...** writes a DDP 2.00 fileset (DDPID, DDPMS, PQDESCR, IMAGE.DAT, CDTEXT.BIN, checksums,
   IMAGE.CUE) for a pressing plant - check it in a DDP player before you send it.
   Projects are saved automatically as .sqmaster files in *Squelchmoth Sessions/Mastering*.

## F3. EQ reference chart (Options → EQ Frequency Chart)

A precise map of where 40 instruments and voices live, grouped by family.

- **Rows**: name, note range (e.g. *E1-G4 41.2-392 Hz*) and four kinds of band, each labelled with its exact
  range: purple = fundamentals / body, green = sweet spot (boost), orange hatched = problem zone (cut),
  yellow-green = air / presence. Under each row the solid line is the note range and the thin line how far
  the harmonics / overtones reach; red shading marks the usual high-pass (and low-pass) point.
- **Scale**: every ISO 1/3-octave centre (20, 25, 31.5, 40 ... 12.5k, 16k, 20k) - the bands of a graphic EQ -
  with octave lines brighter and 100 Hz / 1 kHz / 10 kHz brightest; the seven regions with their ranges
  (Sub 20-60 Hz, Bass 60-250 Hz, Low mids 250-500 Hz, Mids 500 Hz-2 kHz, Upper mids 2-4 kHz, Presence 4-6 kHz,
  Brilliance / Air 6-20 kHz); and a piano keyboard A0-C8 lined up with it (middle C 261.6 Hz, A4 = 440 Hz).
- **Hover** anywhere for the exact frequency, nearest note and cents, MIDI note, 1/3-octave band, region,
  wavelength (m / cm and ft / in) and period (ms), plus every instrument band at that frequency. Hover a row
  for its full notes in the side panel; click a row to pin it; click the scale to pin a frequency (matching
  bands light up, the rest dim); right-click, Esc or **Clear** to reset.
- **Find**: an instrument (*snare*), a problem (*mud*, *sibilance*, *boxy*), a frequency (*300*, *4k*,
  *3.15 kHz*) or a note (*A4*). The family menu shows one family. Scroll with the wheel, arrows, Page Up/Down.
- **Save as PNG** writes the whole chart, every row, at high resolution.
- Claude: **eq_reference** looks up an instrument, problem, frequency or note and can open the chart on it.

## F4. The Beat page (FL Studio's workspace, one key away)

Squelchmoth has four pages on the transport bar: **EDIT · MIX · BEAT · CLAUDE**. **Cmd+Shift+B** opens the Beat page (View >
Beat Page; **Cmd+=** cycles the pages). It is the FL Studio workflow on the same session: nothing here is a separate
project. A channel in the rack is a track in the Edit and Mix windows; a pattern clip on the Beat page is a clip on the
timeline; the Beat Mixer is the Mix window's strips.

- **Channel Rack** - opens automatically as its own floating window the moment you land on the Beat page (the FL window
  on top of the FL page). Close it and it stays closed until you come back or press **Cmd+6** (Window > Channel Rack).
- **Browser** (left) - instruments, your VST3 / AU instruments, the 17 kits, presets by category, samples (your folders:
  right-click to add one). Double-click or drag onto the rack; **Alt-click** previews a sample.
- **Playlist** (centre) - one lane per pattern over 32 bars. Click or drag to place a pattern (linked Pattern Clips,
  one per channel, land on the timeline); right-click a clip for Make Unique, Repeat, Delete. **PAT / SONG** on the
  transport: Pattern mode loops the pattern you're editing through the clip launcher, Song mode plays the timeline.
- **Instrument panel** (right, View > Beat Page > Instrument panel) - the selected channel's instrument embedded.
  VST / AU instruments open in their own window instead.
- **Beat Mixer** (bottom) - the rack channels' strips. It is the Mix window, filtered.

**Edit + Mix at the same time (not one or the other like Pro Tools).** View > Layout: **One page at a time**,
**Edit + Mix (stacked)**, **Edit + Mix (side by side)** - drag the divider; **Mix / Beat / Claude in its own window**
(tear-off: drag it to a second display). **Window > Layouts**: five layout slots (**Cmd+Opt+1-5** recalls; Save Layout 1-5
remembers page, split, divider and torn-off windows) and presets: Tracking, Mixing, Beatmaking, Mastering, Two displays.
Torn-off windows come back where you left them on the next launch.

**Make a beat in 60 seconds** - File > **Make a Beat** (or the Quick Start window's Make a Beat button): new session, a
kit in the rack, the Beat page open and the rack on top. Then: click steps (or **BEAT STARTER** > a genre), **+** > an
instrument, **PIANO ROLL** on it, **Chords** > a progression, place the pattern in the Playlist, **PAT** to loop or
**SONG** to play the arrangement, File > Bounce. Claude does the same with `rack`, `pattern`, `chords`, `bounce`.

## F5. The Channel Rack (FL Studio's rack, Squelchmoth's theme)

Window > **Channel Rack** (**Cmd+6**), or automatically on the Beat page. Same layout as FL Studio, left to right:

**Toolbar**: pattern selector (**<** name **>**, **+** new; right-click: rename, colour, clone, delete, select unused),
**SWING** 0-100 (odd 16ths pushed late, FL's formula), **STEPS** (pattern length 4-512), **DIV** (1/8, 1/16, 1/32,
triplets), **PAT / SONG**, **GRAPH** (Graph Editor, **Cmd+K**), **KEYS** (Keyboard Editor), **PIANO ROLL**, **BEAT STARTER**,
**ZIP** (collapse all rows), **CLAUDE**. Footer: **+** adds a channel (instruments, your plug-ins, kits, a sample, Layer,
MIDI Out) and the **display filter** (channel groups).

**A row**: mute lamp (Cmd-click solo, Shift-click lock for the Beat Starter) · **pan** and **volume** knobs (drag,
double-click resets) · mixer target (the track number in the Edit / Mix window - click to select it there) · the
**channel button** (left-click opens the instrument, double-click opens Channel Settings, right-click the channel menu:
rename, colour, clone, delete, move up / down, zip, group, cut group, send to piano roll / back to steps, Fill, Euclid,
rotate, dice, humanise, double, halve, row length, copy / paste, clear) · selector LED · **16 steps per bar** in four
colours of four (left-click on, right-click off, drag paints, Shift-drag moves the row, **Alt-click accent**, Alt-hover
auditions, **double-click a step** for its roll / chance menu). A channel that holds a piano-roll clip shows the notes
as a mini preview instead of steps; click it to open the piano roll.

**Graph Editor** (Cmd+K): per-step lanes for the selected channel - **Note**, **Velocity**, **Release**, **Fine Pitch**
(+-100 cents), **Pan**, **Mod X**, **Mod Y**, **Shift** (micro-timing), **Repeat** (rolls), **Chance**, **Gate**. Drag to
draw, right-drag for a ramp, Alt-click resets, Ctrl-drag scales the whole lane. **Keyboard Editor**: the Note lane as a
mini keyboard per step; Shift-click adds chord notes (up to four per step).

**Patterns**: every pattern is a scene of the whole rack. **Place** it in the Playlist (or `pattern place`) and every
channel gets a **Pattern Clip** on its track at that bar - linked, so editing the pattern changes every copy. Right-click
a clip > **Make Unique** to break one copy off. Patterns can be any length (STEPS x DIV); song length is the timeline.
**Pattern mode** loops the selected pattern; **Song mode** plays the timeline. The bounce always renders the song.

**Channel Settings** (double-click a channel button) is the **Rack Channel** plug-in sitting at slot 0 of the channel's
track - so it shows in the Mix window too and is automatable: Levels (Mod X / Mod Y, pitch, velocity), Polyphony (max
voices, mono, legato, porta + slide time), Time (gate, shift), Tracking (velocity and key to pan / cut / mod), Cut
(cut group, cut by, cut self - closed hats cut by open hats), **Arpeggiator** (up, down, up-down, random, as played;
time, gate, range, repeat, chord), **Echo** (count, time, feedback, pan, ping-pong, pitch, cut) and Layer mode.

**Channel types**: Instrument (built-in or VST3 / AU), **Sample Channel** (a file on Hive Pads), **Kit** (one channel per
sound - all 12, or `count` 8 for the FL starter order Kick, Snare, Clap, Closed Hat, Open Hat, Perc, 808, Rim), **Layer** (plays its children), **MIDI Out**,
Plugin Host. Delete the track in the Edit window and the channel goes; Undo brings both back.

## F6. Instruments (the shelf)

Rack **+** menu, the Beat page Browser, Track > New, or `rack add` / `instrument insert`. 27 built-in instruments, each
with its own faceplate and 40+ presets by category (**Presets** menu in the window header, grouped: Bass, Lead, Pad,
Keys, Pluck, Arp, FX...), **A/B**, macros, an 8-slot **mod matrix** (click a source or destination, drag the depth,
double-click clears), **Eco / HD** quality (HD oversamples 2x) and voice settings (poly / mono / legato, glide mode,
unison 1-8, max voices, bend range):

**Mothsynth 2** (the flagship analogue-style synth) · **Venom** (trap / drill / dance, trance gate) · **Sim Moth** (simple
analogue) · **Mini Moth** (one screen, under 1% CPU per voice - for 50-channel beats on an Intel Mac) · **Thump** (kick synth) ·
**Dial 10** (2-op FM) · **Blossom 2** (physical-model strings: coupled strings, koto, harp, sitar, bowed pads) · **Hive Pads**
(64-pad layered sampler: Load Kit, Load Sample, Slice loop to pads, choke groups, drag files in) · **Moth 808** · **Mothsynth** ·
**Chrysalis Sampler** · **Hive Drums**.

New in 1.8.1: **Hexwing** (six-operator FM / RM / subtractive hybrid) · **Toxin FM** (sequenced FM) · **Lantern** (vintage
polysynth) · **Lantern Exo** (wavetable + analogue) · **Larva Bass** (acid bass with its own sequencer) · **Thorn Bass**
(electric bass model) · **Carapace** (16-sound drum synth) · **Live Drum Synth** · **Drum Pad Model** · **Thump Live**
(drawable kick envelopes) · **Chrysalis 2** and **Chrysalis SF** (multi-zone sampler, SFZ / SF2) · **Mandible** and
**Mandible Lite** (slicers) · **Pollen** (preset packs). 27 instruments in all; docs/PLUGINS.md has the table per instrument.

## F6b. Samplers, slicers and the 1.8.1 effects

- **Chrysalis 2**: drop WAVs or a whole folder on the window - root notes come from the file names ("Piano C4.wav", "...-60")
  or the WAV's own root, and the keyboard splits between them. **IMPORT** opens an .sfz or .sf2. Every zone has key and
  velocity ranges, tune, gain, pan, loop with crossfade and round-robin group. Factory sounds: the 27 kits and their 808s.
- **Chrysalis SF**: opens a SoundFont and lists its presets; the factory banks are SoundFonts Squelchmoth writes from the kits.
- **Mandible**: drop a loop on deck A (C1 up) or B (C4 up); slices by transients (sensitivity) or a grid (1/4 to 1/32,
  triplets); per-slice pitch, reverse, level, filter, decay; **Stretch** follows the song tempo; **Dump** writes one note
  per slice into a MIDI clip (plays the loop back sample-exact); **To Channels** puts every slice on its own rack channel.
  Factory loops: 17 genres x 3 kits.
- **Time Weaver** (insert on any track): two bars of 32 steps; pick a time slot (half time, stutter, reverse, tape stop,
  scratch...) and a volume slot (gates, pumps, trance gates); MIDI notes from C1 / C4 switch slots live.
- **Larva Filter**: eight filter stages in series, parallel or 4 + 4, each with type, cutoff, resonance, LFO, envelope
  follower and its share of the 16-step sequencer.
- **Apex Triad**: three-band maximiser - lower a band's threshold to make it louder and denser; the ceiling holds the peak.

## F6c. Cut Lab (the audio editor)

Window > Cut Lab, or right-click an audio clip > **Open in Cut Lab**. Drag to select, Space plays / stops, Delete cuts;
Cmd + X / C / V / Z / A / S / R / N / T = cut, copy, paste, undo, select all, save, record, normalise, trim. FILE: Open, Save, Save As. EDIT: cut, copy, paste, trim, silence, fade in / out,
normalise, gain, reverse, remove DC, undo / redo. PROCESS: Denoise, Convolve (pick an impulse response), Blur, Stretch /
Pitch. MARKERS: loop from the selection, add a marker (saved in the WAV). **Send to Timeline** drops the result on the
selected track at the edit cursor. Claude: `cut_lab` (info, edit with a list of operations, open).

## F7. Chords (the piano roll's chord tools)

In the MIDI Editor (double-click a MIDI clip, or **PIANO ROLL** in the rack):
- **Chord stamp**: pick a chord type (21) and the mode - **Bottom-up** (the key you click is the root; choose the
  inversion) or **Top-down** (the key you click stays on top as the melody note and the chord is built under it).
- **Progression Tool** (button, or Chords > Chord Progression Tool...): pick the key and major / minor, filter the library
  by genre (Trap / Drill, R&B / Neo-Soul, Gospel, Lo-Fi, Afro, Dance, Cinematic, Pop, Emotional, Jazz, Latin, Phonk,
  Classic - 50+ progressions) or type your own (`I - V - vi - IV`, `1 5 6 4`, `ii7 V7 Imaj7`). The chord strip shows every
  chord with its roman numeral and notes: click a tile, **Degree - / +** nudges it through the scale, **Quality** forces a
  type, **< Move >** or drag reorders, **+ Add / - Remove**. Options: bars per chord, sevenths, bass / 808 line, voice
  leading, strum, humanise, octave, velocity, replace the clip's notes. **Preview** plays it on the clip's track in
  time with the session; **Insert** writes it (and sets the clip's key).
- **Chords** menu: the quick write - a progression by mood straight into the clip.
- **Key labels**: on a drum channel the keyboard shows the pad / sound names (Kick, Snare, Closed Hat...) automatically;
  right-click any key > **Rename** for your own labels (saved with the track), or Select every note on this key.
- Claude: `chords` (genres, list, preview, generate, stamp, nudge, detect, labels). Chord Track (a session-wide chord lane
  with Follow per channel) comes in 1.8.2.

## G. What Claude can do (124 tools + every menu command)

Session: get_session, new_session, open_session, save_session, list_sessions, undo
Transport: transport, set_loop, set_tempo, set_click, **set_record_mode** (normal / loop / quickpunch, pre/post-roll, count-off, wait for note, MIDI merge)
Session Setup and time: **session_setup** (sample rate 44.1-192 kHz, record bit depth, timecode rate and start, pan depth), **tempo_map** (tempo and meter changes), **rulers** (which rulers show; main / sub counter formats)
Safety: **backups** (Auto-Save and Session File Backups: status, list, back up now, restore, settings), **uninstall_info** (explains uninstalling - Claude never uninstalls)
Mixing: **mix_snapshot** (A-H: store, recall with optional glide, compare, undo), **insert_chain** (save / load whole insert chains), **polarity**, **export_stems**, **track_height**
Edit window: **set_edit_mode** (shuffle / slip / spot / grid, relative grid, grid combined with Shuffle or Slip, Shuffle Lock, grid & nudge values), **set_tool**, **set_selection**, **get_selection**,
  **edit_command** (separate, clear, delete_close_gap, cut, copy, paste, duplicate, repeat, fade, trim, strip_silence, consolidate, heal, nudge, select_all, insert_silence, separate_at_transients, normalize_clip_gain, reverse), **spot_clip**
Tracks: **new_tracks** (the New Tracks dialog: any mix of audio / aux / master fader / VCA / MIDI / instrument / folders, mono or stereo, samples or ticks, or a saved track preset),
  **track_options** (active / inactive, hide / show, record safe, solo safe, input-only monitoring, freeze, commit, bounce, split into mono, duplicate,
  timebase, comments), **set_input** / **set_output** (Pro Tools path names: "In 3", "In 1-2", "Vocal Mic", a bus, a track, "new_aux", "master"),
  **io_setup** (list, rename inputs / outputs / busses, mono-stereo layout, default names, new bus), **edit_window_views**, **master_fader**,
  add_track, delete_track, update_track, list_inputs, **set_automation_mode**, **set_elastic_audio**, **create_folder**, **move_track_to_folder**, **list_folders**, **mix_group**, **playlists**, **track_preset**, **set_colour**
Markers: **add_marker**, **list_markers**, **edit_marker**
Clips: list_clips, import_audio, edit_clip, split_clip, delete_clip, duplicate_clip, **set_clip_gain**, **audiosuite** (rendered effects)
MIDI: create_midi_clip, add_midi_notes, get_midi_notes, quantize_midi, transpose_midi, **midi_tools** (key, chord progressions with bass, chord stamps,
  quantise with swing, humanise, strum, arpeggiate, flam, legato, 808 slides, chop, snap / transpose in key, velocity, flip, riff starter, open the editor)
Channel Rack (1.8): **rack** (list, add instrument / kit / sample / layer / midiout / add_existing, remove, clone, move, set, step, write, lane, fill, euclid, rotate, clear, dice, humanise, double, halve, length, copy, paste, starter, to_piano_roll, to_steps, notes, audition, open),
  **pattern** (list, new, clone, rename, colour, delete, select, settings, place, make_unique, sync, mode pattern / song, select_unused, find), **channel_settings** (get / set / reset: levels, polyphony, time, tracking, cut, arp, echo, layer),
  **instrument** (list, insert, parameters, set, preset, randomise, macros, mod, voice, compare), **chords** (genres, list, preview, generate, stamp, nudge, detect, labels), **kits** (list, sounds, load, render, channels, packs)
Beats: add_drum_track (17 kits), **set_drum_kit**, **beat_lab** (Beat Starter genres, patterns A-H, chain, steps / resolution, swing, per-step velocity /
  gate / chance / shift / repeat, fill / Euclidean / rotate / dice / humanise, row length, mute / solo / lock, to piano roll, open), set_beat, get_beat, load_drum_sound
Plugins: list_plugins (Signature / AirWindows / yours), scan_plugins, insert_plugin, remove_plugin, bypass_plugin, move_plugin,
         get_plugin_parameters, set_plugin_parameters, plugin_presets, open_plugin_window
Mix: add_send, write_automation, clear_automation, get_meters, analyze_mix, bounce
Audio interface and performance: **playback_engine** (device type, device, sample rate, H/W buffer size, input / output channels, host processors, CPU usage limit, ignore errors, dynamic plug-in processing, low-buffer optimisation, overload protection, disk cache), **system_usage** (load, peaks, dropouts, memory, heaviest plug-ins), **crash_recovery** (what happened last time, Safe Mode, plug-ins switched off)
Look & feel: **set_theme**, **set_preference**, **show_window** (eqchart, mastering...), **list_windows**, screenshot, show_view
Mixing reference: **eq_reference** (instrument, problem word, frequency or note: exact ranges, notes, what lives at a frequency; open_chart shows it)
Menus: **menu_list** (every command in the menu bar, with greyed-out / checked state) and **menu_command** - Claude can run any
  menu command by its path, exactly as if you clicked it: "Window > Transport", "Clip > Clip Gain > +3 dB", "Edit > Edit Mode > Grid"
Mastering: **mastering_status**, **mastering_new**, **mastering_add_tracks** (files or the current session), **mastering_set_track**,
  **mastering_set_album**, **mastering_detect_loudness**, **mastering_insert** (track or master rack, with a preset), **mastering_master_fader**,
  **mastering_transport**, **mastering_meters**, **mastering_publish** (digital release with a loudness target, image + CUE, DDP)
Vocals: **moth_tune** (add / get / set / key / detect_key / harmony / presets / open; Graph: graph_analyse, graph_list, graph_correct, graph_edit, graph_clear, open_graph),
  **moth_tune_fx** (Motion patterns and steps, the six-slot X/Y rack, swap, presets)
Pro dynamics: **apex** (dynamics / limiter / multiband / sub: add, get with live loudness and true peak, set, key input, match a loudness target, reset the meter, presets)
Colour plug-ins: **studio_fx** (Aurora Verb, Opto Leveler, Pulse EQ, Oil Can Delay, Silk Ladder, Halo Ring, Twelve Phase, Bucket Brigade: add, get, set, presets, open)
Time & pitch: **molt** (render a clip or several tracks phase-coherently: length by %, ratio, seconds or tempo; semitones and cents; formant; presets)
Presets: plugin_presets lists and loads every Signature plugin's Basic / Advanced bank by name

## H. Keyboard (Pro Tools map)

Space play/stop · Cmd+Space / F12 record · Return RTZ · Shift+Space play from start · L loop · C click ·
Cmd+Shift+L Loop Playback · Cmd+K pre-roll · Cmd+Shift+K post-roll · Q / Shift+Return new memory location ·
Page Up/Down previous/next marker · numeric keypad 0–9 transport (0 play, 3 record, 4 loop, 5 loop record,
6 QuickPunch, 7 click, 8 count-off, 9 MIDI merge).
F1–F4 edit modes · F5–F10 tools · Cmd+E separate · Cmd+D duplicate · Opt+R repeat · Cmd+F fades ·
Cmd+U strip silence · Cmd+H heal · Cmd+T trim to selection · A / S trim start/end · Opt+Shift+3 consolidate ·
Cmd+M mute clip · Cmd+X/C/V · Delete clear · Shift+Delete delete & close gap · Cmd+A select all · Esc deselect ·
+ / − nudge · , / . nudge by grid · Tab / Opt+Tab next/previous transient or clip edge.
Cmd+] / Cmd+[ zoom · T / R zoom · Opt+F fit selection · Cmd+Opt+F fit session · Ctrl+Up/Down selected track heights ·
Cmd+Opt+Up/Down all track heights ·
Cmd+= cycle Edit/Mix/Beat/Claude · Cmd+Shift+B Beat page · Cmd+6 Channel Rack · Cmd+K Graph Editor (in the rack) · Cmd+Opt+1-5 recall layouts · Cmd+Shift+N new track · Cmd+Shift+Delete delete tracks · Shift+S/M/R solo/mute/arm ·
Up/Down (P / ;) select tracks · Cmd+1/2/3/5 windows · Cmd+, Preferences · Cmd+W close floating windows.
Full list: **Help → Keyboard Shortcuts…**

## I. Windows and future builds (GitHub, free)

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

## J. Files and folders

- Sessions: `~/Documents/Squelchmoth Sessions/` (`.squelch` files; Auto-Save every 2 minutes, written safely and never
  while playing or recording - Setup → Preferences → Operation)
- Backups: `Session File Backups/` next to the sessions (every 3 minutes when something changed, keeps the newest 20) plus
  a second copy in `~/Library/Application Support/Squelchmoth/Session Backups/`. File → Restore from Backup... opens one as
  a new "(restored)" session; File → Back Up Now makes one immediately. After a crash, Squelchmoth offers the newest backup.
- Insert chains: `~/Documents/Squelchmoth/Insert Chains/` (`.sqmchain`; click any insert slot → Insert chain)
- Stems: `~/Documents/Squelchmoth Sessions/Bounces/<session> Stems <date>/` (File → Export Stems...)
- Templates: `~/Documents/Squelchmoth Sessions/Templates/` (File → Save As Template)
- Track presets: `~/Documents/Squelchmoth Sessions/Track Presets/`
- Bounces: `~/Documents/Squelchmoth Sessions/Bounces/`
- Sound kits: `~/Documents/Squelchmoth Sessions/Kits/<kit name>/` (24-bit WAVs, rendered on first use)
- Saved plugin presets: `~/Documents/Squelchmoth Sessions/Presets/<plugin>/`
- Settings, theme and the Claude secret: `~/Library/Application Support/Squelchmoth/`

See `PLUGINS.md` for the Signature bundle, the instruments, the kits and Cut Lab.
