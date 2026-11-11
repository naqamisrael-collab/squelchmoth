#!/usr/bin/env python3
"""Build the squelchmoth.com store site into site/ from the app's own source of truth.

Products, prices, the Demo set and the pack contents come from Source/Edition.h; plug-in names, taglines and
artwork from Source/Signature.h; the legal pages from scripts/pkg/license.txt and docs/legal/*.txt. So the site can
never disagree with what the app unlocks. Buy buttons are filled at run time by site/store.js (the one file to edit
when the Payhip products exist).

    python3 scripts/build_site.py            # writes site/*.html, site/style.css, site/store.js (if missing), site/img/art/*
"""
import html, os, re, shutil, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, 'site')
SRC = os.path.join(ROOT, 'Source')
E = html.escape

# ----------------------------------------------------------------------------------------------- data from the app
sig = open(os.path.join(SRC, 'Signature.h')).read()
ROWS = re.findall(r'\{ *"([^"]+)", *"([A-Za-z0-9_]+)", *"([^"]+)", *"([^"]*)", *0x([0-9a-fA-F]{8}), *"([^"]*)" *\}', sig)
ed = open(os.path.join(SRC, 'Edition.h')).read()
PRODUCTS = re.findall(r'\{ *"(\w+)", *"([^"]+)", *"(\$[\d.]+)", *"(\$[\d.]+)", *"([^"]+)" *\}', ed)
PACK_OF = dict(re.findall(r'\{ "(\w+)", Pack::(\w+) \}', ed))
DEMO_TYPES = re.findall(r'"(\w+)"', re.search(r'StringArray demo \{(.*?)\};', ed, re.S).group(1))
DEMO_TRACKS = int(re.search(r'demoMaxTracks = (\d+)', ed).group(1))
PACK_CODE = {'vocal': 'VOCAL', 'color': 'COLOR', 'mastering': 'MASTER', 'instruments': 'INSTR', 'beat': 'BEAT', 'creative': 'CREATIVE'}
AW_COUNT = 186
N_TOOLS = len(set(re.findall(r'^\s*add \("([a-z_0-9]+)"', ''.join(open(os.path.join(SRC, f)).read() for f in os.listdir(SRC) if f.startswith('McpTools')), re.M)))
assert len(PRODUCTS) == 8 and len(ROWS) >= 80, (len(PRODUCTS), len(ROWS))

def clean(t):   # plug-in copy never names other products; only the comparison table names competing DAWs
    t = t.replace('FL-style ', '').replace(' - ', ' — ')
    return t

PLUGINS = []
for name, typ, cat, tag, col, art in ROWS:
    owner = PACK_CODE.get(PACK_OF.get(typ, ''), 'DEMO' if typ in DEMO_TYPES else 'CORE')
    PLUGINS.append(dict(name=name, type=typ, cat=cat, tag=clean(tag), colour='#' + col[2:], art=art, owner=owner))
P = {p[0]: dict(code=p[0], name=p[1], price=p[2], list=p[3], blurb=p[4]) for p in PRODUCTS}
def by(owner): return [p for p in PLUGINS if p['owner'] == owner]
N_SIG = len(PLUGINS); N_DEMO_SIG = len(by('DEMO')); N_DEMO_AW = len([t for t in DEMO_TYPES if t.startswith('airwindows_')])
N_CORE_SIG = len(by('CORE'))
money = lambda s: float(s.replace('$', ''))
PACK_SUM = sum(money(P[c]['price']) for c in PACK_CODE.values())
OFF = int(max(100 * (1 - money(P[c]['price']) / money(P[c]['list'])) for c in PACK_CODE.values()))
SAVE = money(P['CORE']['price']) + PACK_SUM - money(P['COMPLETE']['price'])

# ----------------------------------------------------------------------------------------------- plug-in artwork
os.makedirs(os.path.join(SITE, 'img/art'), exist_ok=True)
for p in PLUGINS:
    src = os.path.join(ROOT, 'Resources/plugart', 'pa_' + p['art'] + '.jpg')
    dst = os.path.join(SITE, 'img/art', p['art'] + '.jpg')
    if p['art'] and os.path.exists(src) and not os.path.exists(dst):
        im = Image.open(src).convert('RGB'); im.thumbnail((540, 360)); im.save(dst, quality=80, optimize=True, progressive=True)
    p['img'] = 'img/art/' + p['art'] + '.jpg' if p['art'] and os.path.exists(dst) else ''
def shot(name):   # real app screenshots (scripts/site_shots.py); a page only uses the ones that exist
    for ext in ('jpg', 'png'):
        if os.path.exists(os.path.join(SITE, 'img/shots', name + '.' + ext)): return 'img/shots/' + name + '.' + ext
    return ''

# ----------------------------------------------------------------------------------------------- shell
NAV = [('index.html', 'Overview'), ('compare.html', 'Compare'), ('addons.html', 'Add-ons'), ('download.html', 'Download'), ('support.html', 'Support')]
def page(fname, title, desc, body, extra_head=''):
    nav = ''.join(f'<li><a href="{h}"{" aria-current=page" if h == fname else ""}>{t}</a></li>' for h, t in NAV)
    return f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{E(title)}</title>
<meta name="description" content="{E(desc)}">
<meta property="og:title" content="{E(title)}"><meta property="og:description" content="{E(desc)}"><meta property="og:image" content="img/shots/edit.jpg">
<link rel="icon" href="img/logo.png">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Syne:wght@500;700;800&family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<link rel="stylesheet" href="style.css">{extra_head}
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<div class="promo">Launch pricing: Squelchmoth <strong>{P['CORE']['price']}</strong> <s>{P['CORE']['list']}</s> · up to {OFF}% off every add-on pack · <a href="compare.html">See editions</a></div>
<header class="nav"><div class="wrap">
  <a class="brand" href="index.html" aria-label="Squelchmoth home"><img src="img/logo.png" alt="" width="34" height="34">SQUELCHMOTH</a>
  <ul>{nav}</ul>
  <div class="navcta"><a class="btn ghost sm" href="download.html">Try free</a><a class="btn primary sm" data-buy="CORE" href="compare.html">Buy</a></div>
</div></header>
<main id="main">
{body}
</main>
<footer><div class="wrap">
  <div class="fcols">
    <div><a class="brand" href="index.html"><img src="img/logo.png" alt="" width="28" height="28">SQUELCHMOTH</a><p>The all-in-one studio for Mac and Windows, with Claude at the console. Created and owned by Jamar Stewart.</p></div>
    <div><h4>Products</h4><a href="compare.html">Squelchmoth</a><a href="compare.html#complete">Squelchmoth Complete</a><a href="addons.html">Add-on packs</a><a href="download.html">Free Demo</a></div>
    <div><h4>Add-ons</h4>{''.join(f'<a href="addons.html#{c.lower()}">{E(P[c]["name"])}</a>' for c in PACK_CODE.values())}</div>
    <div><h4>Help</h4><a href="support.html">Support &amp; FAQ</a><a href="support.html#activate">Activate a key</a><a href="legal.html#refunds">Refunds</a><a href="mailto:support@squelchmoth.com">support@squelchmoth.com</a></div>
    <div><h4>Legal</h4><a href="legal.html#eula">Licence agreement</a><a href="legal.html#privacy">Privacy</a><a href="legal.html#notices">Third-party notices</a></div>
  </div>
  <p class="fine">© 2026 Jamar Stewart. All rights reserved. Squelchmoth is powered by Tracktion Engine and made with JUCE, and includes the AirWindows collection (MIT). Checkout, tax and key delivery by Payhip. Prices in US dollars; tax added at checkout where it applies.</p>
</div></footer>
<script src="store.js"></script>
</body>
</html>
'''

def buy(code, label=None, cls='btn primary'):
    label = label or f'Buy {P[code]["price"]}'
    return f'<a class="{cls}" data-buy="{code}" href="compare.html">{E(label)}</a>'
def price(code, big=True):
    return f'<div class="price{" big" if big else ""}">{P[code]["price"]} <s>{P[code]["list"]}</s></div>'
def figure(src, alt, cap='', cls='shot'):
    if not src: return ''
    return f'<figure class="{cls}"><img src="{src}" alt="{E(alt)}" loading="lazy">{f"<figcaption>{E(cap)}</figcaption>" if cap else ""}</figure>'
def plugcards(items, small=False):
    out = []
    for p in items:
        img = f'<img src="{p["img"]}" alt="" loading="lazy">' if p['img'] else ''
        out.append(f'<article class="pcard" style="--c:{p["colour"]}"><div class="pimg">{img}</div><div class="pbody"><div class="k">{E(p["cat"])}</div><h3>{E(p["name"])}</h3>{"" if small else f"<p>{E(p[chr(116)+chr(97)+chr(103)])}</p>"}</div></article>')
    return f'<div class="pgrid{" small" if small else ""}">' + ''.join(out) + '</div>'

PACK_COPY = {
    'VOCAL': ('vocal', 'A whole vocal studio, from first take to finished record.', 'Seventeen vocal plug-ins that work as one: a tuner with a Live engine under 1 ms and a Studio engine with Graph-mode note editing; a creative tuning rack with 21 modules and per-slot triggers; key and tempo detection; a vocal compressor and a pitch-tracking dynamic EQ; a de-esser that tells T from S; a vocal reverb whose Auto-EQ follows the singer; 45 microphones to convert between; a vocal-tract modeller; breath control; a four-voice harmony choir of up to 32 singers per voice; a doubler that adds vibrato only on straight notes; an extreme voice designer; a 64-band vocoder with a built-in tuner; a talk box; and a one-knob cleaner for hum, noise, room and clicks.', 'voxmoth'),
    'COLOR': ('color', 'Colour, movement and space with real character.', 'Sixteen plug-ins built on new engines: ten-model saturation oversampled to 16x, a 35-style echo, a steel and gold plate reverb, tube colour, a crushing limiter, a broadcast-console EQ, tremolo and auto-pan with rhythm editors, micro-shift widening, a late-70s digital delay, granular pitch-delay, a tap-cloud reverb, a voice changer, an eight-stage filter, a 24-stage phaser and an eight-slot effects rack with 200 chains.', 'cinder'),
    'MASTER': ('master', 'Loud, clean, finished.', 'The Apex series — dynamics, true-peak limiter with a full loudness meter, three-band maximiser, up to six-band multiband and pitch-tracked sub — plus a passive program EQ and an optical leveller with a real VU.', 'apexlim'),
    'INSTR': ('instr', 'Ten instruments with real depth.', 'Six-operator FM, sequenced acid FM, a vintage polysynth and its wavetable sibling, a physical string model, electric and acid basses, a keyed multi-zone sampler with SFZ/SF2 import, a SoundFont player, and Pollen with 11 preset packs.', 'hexwing'),
    'BEAT': ('beat', 'Drums from scratch, loops in slices.', 'A 16-sound drum synth, an acoustic-leaning live drum synth, a physically-modelled drum per instance, a kick synth with drawable envelopes, two loop slicers — plus ten more sound kits and 51 genre loops.', 'carapace'),
    'CREATIVE': ('creative', 'Movement, texture and happy accidents.', 'A time and volume slicer with 72 factory slots, an eight-stage filter with its own sequencer, oil-can and bucket-brigade delays, a ring modulator, a twelve-stage phaser and a transistor-ladder filter.', 'timeweaver'),
}

# ----------------------------------------------------------------------------------------------- index
def tiles():
    t = f'''<div class="tiles">
  <article class="tile"><div class="eyebrow">Free forever</div><h3>Squelchmoth Demo</h3><div class="price big">$0</div>
    <p>Learn the program and make real songs: {DEMO_TRACKS} tracks, {N_DEMO_SIG} basic instruments and effects, five sound kits, save and bounce your mix.</p>
    <a class="btn" data-buy="DEMO" href="download.html">Download the Demo</a></article>
  <article class="tile featured"><div class="eyebrow">Most popular</div><h3>Squelchmoth</h3>{price('CORE')}
    <p>The full program: unlimited tracks, {N_DEMO_SIG + N_CORE_SIG} Signature instruments and effects, the {AW_COUNT}-effect AirWindows collection, 17 kits, Mastering, Cut Lab, stems and DDP.</p>
    {buy('CORE')}</article>
  <article class="tile"><div class="eyebrow">Best value · save ${SAVE:.0f}</div><h3>Squelchmoth Complete</h3>{price('COMPLETE')}
    <p>Squelchmoth and all six add-on packs: every one of the {N_SIG} Signature plug-ins, all 27 kits and 51 loops.</p>
    {buy('COMPLETE')}</article>
</div>'''
    return t

CLAUDE_HTML = '\n<section class="band claudeband" id="claude"><div class="wrap">\n  <header class="sh wide"><div class="eyebrow">Claude at the console</div><h2>Talk to your session like you\'d talk to your engineer.</h2>\n    <p>Squelchmoth was built to be run by Claude. Connect it once and Claude reaches {N_TOOLS} tools and every menu command: tracks, clips, MIDI, every plug-in parameter in its own units, sends, automation, the mastering window and every export. You describe the sound. Claude makes the moves, checks the meters and tells you what it changed. Every action lands in the log, and Undo takes back anything you don\'t like.</p></header>\n  <div class="asks">{asks}</div>\n  {claudefig}\n  <div class="split claudesplit">\n    <div><h3>Two ways to work with Claude</h3>\n      <ol class="steps"><li><strong>Squelchmoth Chat, right in the app.</strong> Press <strong>CHAT</strong>, paste your Anthropic API key once, and type. Claude works in the open session and shows every move it makes as it goes. No browser needed. Usage is billed by Anthropic to your API account.</li><li><strong>Or from claude.ai.</strong> Press <strong>Connect Claude</strong>, copy the private link, and add it in claude.ai under Settings › Connectors. Then ask from any Claude chat.</li><li>Either way, Undo takes back anything you don\'t like, and every action lands in the log.</li></ol></div>\n    <div><h3>Built for trust</h3>\n      <ul class="ticks"><li>Works with your own VST3 and Audio Unit plug-ins: Claude reads their parameters and sets them by name</li><li>Uses your own Claude or Anthropic account. Your music stays on your computer, and your API key never leaves it.</li><li>Locked features stay locked for Claude too, and it tells you what would unlock them</li></ul></div>\n  </div>\n</div></section>\n'
ASKS_HTML = '<article class="ask"><p class="say">“Make the lead vocal sit on top: bright, no harsh S&#x27;s, a little room.”</p><p class="does"><span class="k">Claude</span> Puts Hush De-Ess on the vocal, lifts the high shelf on Prism EQ, raises the fader 1.5 dB, sends 15 dB under to the reverb bus, then reads the meters back to you.</p></article><article class="ask"><p class="say">“Glue the drums and make the kick punch through the 808.”</p><p class="does"><span class="k">Claude</span> Adds Pupa Buss Comp to the drums, Snap Transient on the kick and a cut on the 808 where the kick lives, then compares the levels before and after.</p></article><article class="ask"><p class="say">“Tune the hook to A minor and keep it natural.”</p><p class="does"><span class="k">Claude</span> Opens Moth Tune on the vocal, sets A minor, slows the retune and keeps the vibrato.</p><span class=chip>Needs Vocal Suite</span></article><article class="ask"><p class="say">“Give me a 140 BPM drill beat with sliding 808s.”</p><p class="does"><span class="k">Claude</span> Sets the tempo, rolls a drill pattern in Beat Lab and writes an 808 line with glides in the piano roll.</p></article><article class="ask"><p class="say">“Sweep a filter into the drop at bar 17.”</p><p class="does"><span class="k">Claude</span> Inserts Veil Filter on the music bus and draws cutoff automation from bar 13 to bar 17.</p></article><article class="ask"><p class="say">“Master the EP for streaming: −14 LUFS, −1 dB true peak.”</p><p class="does"><span class="k">Claude</span> Builds the album in the Mastering window, sets the limiter target, measures every song and exports tagged release files.</p><span class=chip>Needs Mastering Suite</span></article><article class="ask"><p class="say">“Bounce stems for my mix engineer.”</p><p class="does"><span class="k">Claude</span> Exports one file per track, same length and start, named after the tracks.</p></article><article class="ask"><p class="say">“Is anything clipping? Show me the mixer.”</p><p class="does"><span class="k">Claude</span> Analyses the mix, names the tracks that peak too hot and sends you a screenshot of the Mix window.</p></article>'
FEAT_HTML = '<div class="fcard"><h3>Claude runs the session</h3><p>{N_TOOLS} tools plus every menu command. Mix, edit, write parts, master and export by conversation.</p></div><div class="fcard"><h3>Four studios in one app</h3><p>A full recording and editing workflow, a beat-making rack, a mixing console and an album mastering page. No hopping between programs.</p></div><div class="fcard"><h3>{N_SIG} Signature plug-ins</h3><p>Instruments and effects with their own interfaces and factory presets, plus {AW_COUNT} AirWindows effects underneath.</p></div><div class="fcard"><h3>27 kits, 51 loops, 5,300+ presets</h3><p>Squelchmoth\'s own drum engine renders every kit. Start a beat in seconds or build from scratch.</p></div><div class="fcard"><h3>Pay once</h3><p>{CORE_PRICE} for a perpetual licence on two computers. Keys work offline and updates within version 1 are free.</p></div><div class="fcard"><h3>Your plug-ins welcome</h3><p>VST3 hosting on Mac and Windows, plus Audio Units on Mac, scanned safely in a separate process. A crashing plug-in gets switched off, and your session reopens.</p></div><div class="fcard"><h3>Mac and Windows</h3><p>macOS 11 Big Sur and newer, native on Intel and Apple Silicon, plus 64-bit Windows 10 and 11. One key works on both.</p></div><div class="fcard"><h3>Checked on every build</h3><p>An automated self-test drives the whole app through Claude\'s tools on every build: more than 1,300 checks, from tuning and preset levels to bounces.</p></div>'
COMP_HTML = '<div class="tablewrap"><table class="cmp vs"><thead><tr><th></th><th>Squelchmoth</th><th>Pro Tools</th><th>Logic Pro</th><th>FL Studio</th><th>Ableton Live 12</th><th>Fender Studio Pro</th></tr></thead><tbody><tr><th scope="row">Price</th><td class=us>{CORE_PRICE} once · free Demo</td><td>Subscription, or $199 (Artist) / $599 (Studio) perpetual</td><td>$199.99 once, or $12.99/mo Creator Studio</td><td>From $99 · Producer $199 · lifetime free updates</td><td>Intro $99 · Standard $349 · Suite $749</td><td>$199.99 once, or $19.99/mo</td></tr><tr><th scope="row">Platforms</th><td class=us>Mac (macOS 11+) and Windows 10/11 · one key for both</td><td>Mac and Windows</td><td>Mac (macOS 15.6+) and iPad</td><td>Mac and Windows</td><td>Mac and Windows</td><td>Mac, Windows and Linux</td></tr><tr><th scope="row">AI that operates your session from a chat</th><td class=us><b>Claude</b>: tracks, MIDI, every plug-in parameter, automation, mastering and bounces</td><td>No chat control (AI transcription and chord symbols)</td><td>No chat control (Session Players write parts; Mastering Assistant)</td><td>Gopher: mixer levels, routing, plug-in parameters and drum patterns; no automation in published tests</td><td>None built in (the Claude connector answers manual questions only)</td><td>Studio Assistant answers questions</td></tr><tr><th scope="row">Album mastering page with DDP export</th><td class=us>Yes</td><td>Not in feature list</td><td>Mastering Assistant per song; DDP not listed</td><td>Not in feature list</td><td>Not in feature list</td><td>Yes</td></tr><tr><th scope="row">Built-in beat-making rack, album mastering and AI control in one app</th><td class=us>Yes</td><td>—</td><td>—</td><td>—</td><td>—</td><td>—</td></tr></tbody></table></div>'

def index():
    packs = ''.join(f'''<a class="packtile" href="addons.html#{PACK_COPY[c][0]}" style="--img:url({next((p["img"] for p in PLUGINS if p["art"]==PACK_COPY[c][3]), "")})">
      <span class="k">{len(by(c))} {"instruments" if c=="INSTR" else "plug-ins"}{" + kits &amp; loops" if c=="BEAT" else ""}</span><strong>{E(P[c]["name"])}</strong><span class="pp">{P[c]["price"]} <s>{P[c]["list"]}</s></span></a>''' for c in PACK_CODE.values())
    feat = [
        ('edit', 'Record &amp; edit', 'Every editing move you already know.', 'Shuffle, Slip, Spot and Grid. Zoomer, Trim, Selector, Grabber, Scrubber, Pencil and the Smart Tool. Playlists, elastic time and pitch, rendered clip effects, memory locations, folders, VCAs and groups — with the shortcuts your hands expect.'),
        ('mix', 'Mix', 'A dynamics console you read at a glance.', 'Every channel carries a COMP and a LIMIT slot with real gain-reduction meters, a segmented LED level meter under a chrome fader, and a live-signal light. Set each strip Compact, Standard or Expanded, meter in Peak, RMS or LUFS, and save mixer layouts with the session. Full view adds ten inserts, ten sends, I/O and pan to every strip. Select several channels and one fader moves them all.'),
        ('beatmaker', 'Produce', 'Beat Lab: pads, patterns, piano roll.', 'A channel rack and step sequencer, 64-pad sampler, slicers, a piano roll that plays as you draw, chord tools and scale snapping. Describe the groove to Claude and it lands on the grid.'),
        ('voice', 'Vocals', 'A complete vocal studio, built in.', 'Tune live under 1 ms or note by note in the graph. Stack a four-voice choir of up to 32 singers per voice, double with vibrato only where the singer holds a straight note, vocode on 64 bands with a built-in tuner, talk through a guitar, and clean hum, noise, room and clicks with one knob. Every vocal plug-in hears the same pitch tracker, and Claude can run all of it.'),
        ('color', 'Colour &amp; space', 'Character you can hear, measured.', 'Ten-model saturation oversampled to 16x with a live harmonic readout, a 35-style echo, steel and gold plates, a crushing limiter, a broadcast-console EQ, granular pitch-delay, tap-cloud reverb and an eight-slot effects rack with 200 ready chains. Over 3,300 new presets, each leaving your sound at the level it came in.'),
        ('mastering', 'Master &amp; deliver', 'Finish the album in the same app.', 'Sequence songs, process each one and the whole record, meter with EBU R128 loudness, true peak and phase, then publish streaming-ready WAV, FLAC or MP3 files, a WAV + CUE image or a DDP fileset for the plant.'),
    ]
    rows = ''
    for i, (s, k, h, p) in enumerate(feat):
        src = shot(s)
        rows += f'''<section class="row{" flip" if i % 2 else ""}"><div class="wrap">
  <div class="rtext"><div class="eyebrow">{k}</div><h2>{h}</h2><p>{p}</p></div>
  {figure(src, h) if src else ""}
</div></section>'''
    sig_core = [p for p in PLUGINS if p['owner'] in ('DEMO', 'CORE')]
    pick = [p for p in sig_core if p['name'] in ('Prism EQ', 'Cocoon Compressor', 'Ember Tape', 'Aurora Verb', 'Mothsynth 2', 'Venom', 'Moth Press', 'Hive Pads')]
    claudefig = figure(shot('chat'), 'Squelchmoth Chat beside the mixer', 'Squelchmoth Chat: you type, Claude changes the session and lists every tool it used.') if shot('chat') else figure(shot('claude'), 'The Claude page in Squelchmoth', 'The Claude page: the link status and a live log of every move Claude makes.')
    body = f'''
<section class="hero"><div class="wrap">
  <div>
    <div class="eyebrow">Digital audio workstation · macOS 11+ · Windows 10 &amp; 11</div>
    <h1>Say how it should sound.<br><span class="acid">Claude mixes it in Squelchmoth.</span></h1>
    <p class="lede">The complete studio for Mac and Windows — record, produce, mix and master — with Claude connected to every track, plug-in, fader and export. Tell it what you hear in your head, in plain words. Claude does the work in your session while you watch.</p>
    <div class="cta">{buy('CORE', f'Buy Squelchmoth — {P["CORE"]["price"]}')}<a class="btn ghost" data-buy="DEMO" href="download.html">Download the free Demo</a></div>
    <p class="fine">Perpetual licence · no subscription · keys work offline · VST3 &amp; Audio Unit hosting</p>
  </div>
  <figure class="shot hero-shot"><img src="{shot('edit')}" alt="The Squelchmoth Edit window" width="1600" height="900"></figure>
</div></section>

<section class="trust"><div class="wrap">
  <div><strong>{N_SIG + AW_COUNT}</strong><span>built-in plug-ins</span></div>
  <div><strong>27</strong><span>sound kits · 51 loops</span></div>
  <div><strong>5,300+</strong><span>factory presets</span></div>
  <div><strong>2</strong><span>computers per licence</span></div>
  <div><strong>30-day</strong><span>works-on-your-computer refund</span></div>
</div></section>

{{CLAUDE}}
<section class="band"><div class="wrap">
  <header class="sh"><div class="eyebrow">Choose your Squelchmoth</div><h2>Start free. Buy the program once. Add only what you need.</h2></header>
  {tiles()}
  <p class="center"><a class="link" href="compare.html">Compare every feature →</a></p>
</div></section>
<section class="band alt" id="features"><div class="wrap">
  <header class="sh"><div class="eyebrow">Why producers switch</div><h2>Everything in one place, and the best engineer you've ever had on call.</h2></header>
  <div class="fgrid">{{FEAT}}</div>
</div></section>
{rows}
<section class="band alt" id="plugins"><div class="wrap">
  <header class="sh"><div class="eyebrow">Signature plug-ins</div><h2>Instruments and effects with faces of their own.</h2><p>Every Signature plug-in has its own interface, a live display, Basic and Advanced factory preset banks, A/B compare and user presets. The AirWindows collection waits underneath for the deep end. Your own VST3 and Audio Unit plug-ins load alongside.</p></header>
  {plugcards(pick)}
  <div class="gallery">{''.join(figure(shot(k), n, n) for k, n in [('prismeq', 'Prism EQ'), ('auroraverb', 'Aurora Verb'), ('embertape', 'Ember Tape'), ('mothsynth2', 'Mothsynth 2'), ('pollen', 'Pollen'), ('lanternexo', 'Lantern Exo')])}</div>
  <p class="center"><a class="link" href="compare.html#plugins">See what each edition includes →</a></p>
</div></section>

<section class="band" id="vs"><div class="wrap">
  <header class="sh wide"><div class="eyebrow">The competition</div><h2>How Squelchmoth compares.</h2><p>The big studios are excellent. Squelchmoth is the one you can mix by talking to it, with beat-making and album mastering in the same app, for one payment.</p></header>
  {{COMP}}
  <p class="fine srcs">Competitor details from vendor pages, major US retailers and published reviews, checked November 6 2026. Prices are in US dollars before tax and change often. Pro Tools, Logic Pro, FL Studio, Ableton Live and Fender Studio Pro are trademarks of their owners. Squelchmoth is independent and not affiliated with them.</p>
</div></section>

<section class="band alt" id="addons"><div class="wrap">
  <header class="sh"><div class="eyebrow">Add-on packs</div><h2>Grow your studio one pack at a time.</h2><p>Add-ons work in every edition — even the free Demo. Buy one, enter its key, and its plug-ins unlock in place with your settings intact.</p></header>
  <div class="packs">{packs}</div>
</div></section>

<section class="band"><div class="wrap">
  <header class="sh"><div class="eyebrow">Questions</div><h2>Before you buy.</h2></header>
  {faq(FAQ_SHORT)}
  <p class="center"><a class="link" href="support.html">All questions and help →</a></p>
</div></section>

<section class="endcta"><div class="wrap">
  <h2>Make the next record in Squelchmoth.</h2>
  <div class="cta">{buy('CORE', f'Buy Squelchmoth — {P["CORE"]["price"]}')}<a class="btn ghost" data-buy="DEMO" href="download.html">Try the free Demo</a></div>
</div></section>'''
    body = body.replace('{CLAUDE}', CLAUDE_HTML.replace('{asks}', ASKS_HTML).replace('{claudefig}', claudefig).replace('{N_TOOLS}', str(N_TOOLS))).replace('{FEAT}', FEAT_HTML).replace('{COMP}', COMP_HTML)
    body = body.replace('{N_TOOLS}', str(N_TOOLS)).replace('{N_SIG}', str(N_SIG)).replace('{AW_COUNT}', str(AW_COUNT)).replace('{CORE_PRICE}', P['CORE']['price'])
    return page('index.html', 'Squelchmoth — mix by talking to Claude. The all-in-one studio for Mac and Windows', f'Record, produce, mix and master on macOS and Windows. {N_SIG} Signature plug-ins, 27 kits, album mastering and Claude control. Free Demo; full program {P["CORE"]["price"]}.', body)

# ----------------------------------------------------------------------------------------------- FAQ
FAQ_SHORT = [
    ('Is the Demo really free forever?', f'Yes. No time limit and no watermark. It has the basics — {DEMO_TRACKS} tracks, {N_DEMO_SIG} instruments and effects, five kits — and you can save sessions and bounce your mix. When you need more, enter a key and the same app unlocks; nothing to reinstall.'),
    ('Is it a subscription?', 'No. Squelchmoth and every pack are one-time purchases with a perpetual licence. Updates within the same major version are free.'),
    ('Can I use my own plug-ins?', 'Yes. Squelchmoth hosts VST3 and Audio Unit plug-ins, scans them safely in a separate process, and sorts them into categories with a search box in every plug-in menu.'),
    ('How do I get my key?', 'Checkout is handled by Payhip. Your key and download link arrive by email straight away. Paste the key into Help › Licences and Upgrades — it works offline.'),
]
FAQ_FULL = FAQ_SHORT + [
    ('What happens to a session that uses a plug-in I don\'t own?', 'It opens normally. The plug-in loads as a Locked panel that keeps every setting and its automation; the panel shows which product unlocks it. Effects pass audio through and instruments stay silent until you unlock and reopen the session — then they come back exactly as they were.'),
    ('Do add-on packs need the full program?', 'No. Packs work in every edition, including the free Demo.'),
    ('How many computers can I use?', 'Two computers that you own or control, used by one person at a time. Mix and match: one key works on Mac and Windows.'),
    ('Does it need the internet?', 'No. Recording, editing, mixing, mastering, exports and key activation all work offline. Only Squelchmoth Chat and the claude.ai connection use the internet.'),
    ('Which computers does it run on?', 'Macs with macOS 11 Big Sur or newer, Intel or Apple Silicon, with a native build for each; and 64-bit Windows 10 and 11. One key unlocks both.'),
    ('What is your refund policy?', 'Try the free Demo first — it runs the same program. If something you bought does not work on your computer, email us within 30 days for a full refund. Refunds cancel the key.'),
    ('What do I need for Squelchmoth Chat?', 'An Anthropic API key from console.anthropic.com. Paste it once in the chat panel; it is stored only on your computer. Anthropic bills API use to that account; a claude.ai Pro or Max plan does not include API use. If you would rather use your claude.ai plan, connect Squelchmoth to claude.ai instead (Claude page › Connect Claude) and chat there.'),
    ('Is Squelchmoth affiliated with Anthropic?', 'No. Squelchmoth is an independent product by Jamar Stewart. Claude is made by Anthropic; Squelchmoth Chat and the claude.ai connection use your own Anthropic or Claude account.'),
]
def faq(items):
    return ''.join(f'<details{" open" if i == 0 else ""}><summary>{E(q)}</summary><p>{E(a)}</p></details>' for i, (q, a) in enumerate(items))

# ----------------------------------------------------------------------------------------------- compare
def compare():
    Y, N = '<td class="y">✓</td>', '<td class="n">—</td>'
    def r(label, d, c, x, note=''):
        cell = lambda v: (Y if v is True else N if v is False else f'<td class="v">{v}</td>')
        return f'<tr><th scope="row">{label}{f"<small>{note}</small>" if note else ""}</th>{cell(d)}{cell(c)}{cell(x)}</tr>'
    groups = [
        ('Tracks &amp; editing', [
            r('Audio, MIDI, instrument, aux, folder and VCA tracks', f'Up to {DEMO_TRACKS}', 'Unlimited', 'Unlimited'),
            r('Shuffle, Slip, Spot, Grid · Smart Tool · playlists', True, True, True),
            r('Elastic time and pitch per track', True, True, True),
            r('Molt — high-quality time and pitch rendering', False, True, True),
            r('Cut Lab audio editor', False, True, True),
            r('Piano roll, chord tools, scale snapping', True, True, True),
            r('Third-party VST3 and Audio Unit plug-ins', True, True, True)]),
        ('Instruments', [
            r('Basic instruments', f'{len([p for p in by("DEMO") if p["cat"]=="Instruments"])}', f'{len([p for p in by("DEMO") if p["cat"]=="Instruments"])}', f'{len([p for p in by("DEMO") if p["cat"]=="Instruments"])}', 'Mothsynth, Sim Moth, Mini Moth, Moth 808, Thump, Chrysalis Sampler, Hive Drums, Hive Pads'),
            r('Flagship synths: Mothsynth 2, Venom, Dial 10', False, True, True),
            r(f'Instrument Collection ({len(by("INSTR"))})', False, 'Add-on', True, 'Hexwing, Toxin FM, Lantern, Lantern Exo, Pollen and more'),
            r(f'Beat Lab Expansion drum synths and slicers ({len(by("BEAT"))})', False, 'Add-on', True)]),
        ('Effects', [
            r('Basic Signature effects', f'{len([p for p in by("DEMO") if p["cat"]!="Instruments"])}', f'{len([p for p in by("DEMO") if p["cat"]!="Instruments"])}', f'{len([p for p in by("DEMO") if p["cat"]!="Instruments"])}', 'EQ, filter, compressor, gate, de-esser, drive, reverb, delay, chorus, phaser, pitch, width'),
            r('Core Signature effects', False, f'{len([p for p in by("CORE") if p["cat"]!="Instruments"])}', f'{len([p for p in by("CORE") if p["cat"]!="Instruments"])}', 'Tape, console, tube, vari-mu, buss comp, convolution, FDN reverb and more'),
            r(f'The AirWindows collection ({AW_COUNT})', f'{N_DEMO_AW}', 'All', 'All'),
            r(f'Vocal Suite ({len(by("VOCAL"))})', False, 'Add-on', True, 'Moth Tune, Moth Tune FX, Double Wing ADT'),
            r(f'Mastering Suite ({len(by("MASTER"))})', False, 'Add-on', True, 'The Apex series, Pulse EQ, Opto Leveler'),
            r(f'Creative FX ({len(by("CREATIVE"))})', False, 'Add-on', True, 'Time Weaver, Larva Filter, Oil Can Delay and more')]),
        ('Sounds', [
            r('Sound kits', '5', '17', '27'), r('Genre loops', False, False, '51'), r('Factory presets for every plug-in you own', True, True, True)]),
        ('Mix, master &amp; deliver', [
            r('Save sessions, bounce a stereo mix (WAV, AIFF, FLAC, MP3 with ID3 tags, LUFS-normalised)', True, True, True),
            r('Stem export', False, True, True),
            r('Mastering window: album sequencing, R128 loudness, true peak', False, True, True),
            r('Digital release (WAV, AIFF, FLAC, OGG, MP3), WAV + CUE image, DDP 2.00', False, True, True),
            r('Track macros, 1/12-octave spectrum, waterfall and sonogram', False, True, True),
            r('Hardware inserts with ping and delay compensation', True, True, True),
            r('Score editor: engraved notation, note entry, MusicXML, PDF', True, True, True)]),
        ('Claude', [r('Claude control over the whole session', True, True, True, 'Locked features stay locked for Claude too')]),
        ('Licence', [r('Price', '$0', f'{P["CORE"]["price"]} <s>{P["CORE"]["list"]}</s>', f'{P["COMPLETE"]["price"]} <s>{P["COMPLETE"]["list"]}</s>'),
                     r('Perpetual licence · 2 computers (Mac or Windows) · free updates within the major version', '—', True, True)]),
    ]
    table = ''.join(f'<tbody><tr class="grp"><th colspan="4">{g}</th></tr>{"".join(rows)}</tbody>' for g, rows in groups)
    body = f'''
<section class="pagehead"><div class="wrap"><div class="eyebrow">Compare editions</div><h1>Which Squelchmoth is right for you?</h1><p class="lede">Every edition is the same app. Start with the free Demo; a key unlocks more in place, with nothing to reinstall.</p></div></section>
<section class="band tight"><div class="wrap">
  <div class="plans3">
    <article class="plan"><div class="eyebrow">Free forever</div><h2>Demo</h2><div class="price big">$0</div><p>The basics for learning and sketching.</p><a class="btn" data-buy="DEMO" href="download.html">Download free</a>
      <ul><li>{DEMO_TRACKS} tracks</li><li>{N_DEMO_SIG} basic instruments and effects + {N_DEMO_AW} AirWindows</li><li>Kits 1–5</li><li>Save and bounce your mix</li><li>Claude control</li></ul></article>
    <article class="plan featured"><div class="eyebrow">Most popular</div><h2>Squelchmoth</h2>{price('CORE')}<p>The full program, perpetual licence.</p>{buy('CORE')}
      <ul><li>Unlimited tracks</li><li>{N_DEMO_SIG + N_CORE_SIG} Signature plug-ins + all {AW_COUNT} AirWindows</li><li>17 kits</li><li>Mastering window, Cut Lab, Molt</li><li>Stems and MP3, digital release, DDP</li><li>Add packs whenever you like</li></ul></article>
    <article class="plan" id="complete"><div class="eyebrow">Best value · save ${SAVE:.0f}</div><h2>Complete</h2>{price('COMPLETE')}<p>Squelchmoth and all six packs.</p>{buy('COMPLETE')}
      <ul><li>Everything in Squelchmoth</li><li>All {N_SIG} Signature plug-ins</li><li>Vocal, Color &amp; Space, Mastering, Instrument, Beat Lab and Creative packs</li><li>All 27 kits and 51 loops</li></ul></article>
  </div>
</div></section>
<section class="band alt" id="plugins"><div class="wrap">
  <header class="sh"><h2>Feature by feature</h2></header>
  <div class="tablewrap"><table class="cmp"><thead><tr><th></th><th>Demo<small>$0</small></th><th>Squelchmoth<small>{P["CORE"]["price"]}</small></th><th>Complete<small>{P["COMPLETE"]["price"]}</small></th></tr></thead>{table}</table></div>
</div></section>
<section class="band"><div class="wrap">
  <header class="sh"><div class="eyebrow">Already own Squelchmoth?</div><h2>Add packs one at a time.</h2></header>
  <div class="packrow">{''.join(f'<a class="mini" href="addons.html#{c.lower()}"><strong>{E(P[c]["name"])}</strong><span>{P[c]["price"]}</span></a>' for c in PACK_CODE.values())}</div>
</div></section>'''
    return page('compare.html', 'Compare editions — Squelchmoth', f'Demo free, Squelchmoth {P["CORE"]["price"]}, Complete {P["COMPLETE"]["price"]}. Compare tracks, plug-ins, kits, mastering and delivery.', body)

# ----------------------------------------------------------------------------------------------- add-ons
def addons():
    sections = ''
    for i, c in enumerate(PACK_CODE.values()):
        anchor, head, lede, flag = PACK_COPY[c]
        items = by(c)
        src = shot(flag)
        names = ', '.join(p['name'] for p in items)
        sections += f'''<section class="band{" alt" if i % 2 else ""} pack" id="{anchor}"><div class="wrap">
  <div class="packhead">
    <div><div class="eyebrow">{E(P[c]["name"])} · {len(items)} {"instruments" if c=="INSTR" else "plug-ins"}{" · 10 kits · 51 loops" if c=="BEAT" else ""}</div><h2>{head}</h2><p class="lede">{lede}</p>
      <div class="buyrow">{price(c)}{buy(c, f'Buy {P[c]["name"]}')}</div>
      <p class="fine">Works in every edition, including the free Demo · key by email · perpetual licence</p></div>
    {figure(src, P[c]["name"] + " — " + next(p["name"] for p in items if p["art"] == flag)) if src else figure(next(p["img"] for p in items if p["art"] == flag), P[c]["name"], cls="shot art")}
  </div>
  {plugcards(items)}
</div></section>'''
    jump = ''.join(f'<a href="#{PACK_COPY[c][0]}">{E(P[c]["name"])}</a>' for c in PACK_CODE.values())
    body = f'''
<section class="pagehead"><div class="wrap"><div class="eyebrow">Add-on packs</div><h1>The add-on store.</h1><p class="lede">Six packs that drop straight into Squelchmoth — or into the free Demo. Buy, paste the key, and the plug-ins unlock where they already sit in your sessions.</p>
  <nav class="jump" aria-label="Packs">{jump}</nav></div></section>
<section class="band tight"><div class="wrap"><div class="bundle">
  <div><div class="eyebrow">Bundle</div><h2>Want all of it? Squelchmoth Complete.</h2><p>The full program and all six packs for {P["COMPLETE"]["price"]} — ${SAVE:.0f} less than buying them one by one.</p></div>
  <div class="buyrow">{price('COMPLETE')}{buy('COMPLETE', 'Buy Complete')}</div>
</div></div></section>
{sections}'''
    return page('addons.html', 'Add-on packs — Squelchmoth', 'Vocal Suite, Mastering Suite, Instrument Collection, Beat Lab Expansion and Creative FX for Squelchmoth.', body)

# ----------------------------------------------------------------------------------------------- download
def download():
    body = f'''
<section class="pagehead"><div class="wrap"><div class="eyebrow">Download</div><h1>Get Squelchmoth for Mac and Windows.</h1><p class="lede">One installer for everyone. Without a key it runs as the free Demo; your key unlocks what you bought, in place.</p></div></section>
<section class="band tight"><div class="wrap dlgrid">
  <article class="plan featured"><div class="eyebrow">Free Demo</div><h2>Download Squelchmoth</h2><div class="price big">$0</div>
    <p>Enter your email and the download arrives straight away — the Mac installers (Intel and Apple Silicon) and the Windows installer, about 45 MB each. You'll also get the occasional free kit and update notes; unsubscribe any time.</p>
    <a class="btn primary" data-buy="DEMO" href="download.html">Get the free Demo</a>
    <p class="fine">Already bought? Use the download link in your purchase email — it is the same installer.</p></article>
  <article class="card"><div class="k">System requirements</div><h3>What you need</h3>
    <ul class="ticks"><li><strong>Mac:</strong> macOS 11 Big Sur or newer, Intel or Apple Silicon (native build for each)</li><li><strong>Windows:</strong> Windows 10 or 11, 64-bit</li><li>200 MB of free disk space</li><li>Any Core Audio (Mac) or Windows Audio interface, or the built-in audio</li><li>Internet only for the optional Claude connection</li></ul></article>
  <article class="card"><div class="k">Install</div><h3>Two minutes</h3>
    <p><strong>Mac</strong></p><ol class="steps"><li>Unzip the download for your Mac.</li><li>Right-click <strong>Install Squelchmoth</strong> and choose <strong>Open</strong>, then Open again.</li><li>It copies Squelchmoth into Applications and offers to open it.</li></ol><p><strong>Windows</strong></p><ol class="steps"><li>Unzip and double-click <strong>Squelchmoth-Setup</strong>.</li><li>If Windows says it protected your PC, click <strong>More info</strong> › <strong>Run anyway</strong>.</li><li>Next › I Agree › Install. Squelchmoth is in the Start menu.</li></ol><p class="fine">Bought a key? Open Help › Licences and Upgrades and paste it.</p></article>
</div></section>
<section class="band alt"><div class="wrap">
  <header class="sh"><h2>What the free Demo includes</h2></header>
  <div class="grid3">
    <div class="card"><div class="k">Included</div><ul class="ticks"><li>{DEMO_TRACKS} tracks of audio, MIDI and instruments</li><li>{N_DEMO_SIG} Signature instruments and effects</li><li>{N_DEMO_AW} AirWindows effects</li><li>Sound kits 1–5, Beat Lab, piano roll</li><li>Save sessions, bounce your mix</li><li>Your VST3 plug-ins (and Audio Units on Mac)</li><li>Claude control</li></ul></div>
    <div class="card"><div class="k">With Squelchmoth</div><ul class="ticks"><li>Unlimited tracks</li><li>{N_CORE_SIG} more Signature plug-ins and the full AirWindows collection</li><li>Kits 6–17</li><li>Mastering window, Cut Lab, Molt</li><li>Stems, MP3 release files, DDP</li></ul>{buy('CORE')}</div>
    <div class="card"><div class="k">With add-on packs</div><ul class="ticks">{''.join(f'<li>{E(P[c]["name"])} — {P[c]["price"]}</li>' for c in PACK_CODE.values())}</ul><a class="btn" href="addons.html">Browse add-ons</a></div>
  </div>
</div></section>'''
    return page('download.html', 'Download — Squelchmoth', 'Download Squelchmoth for macOS 11+ (Intel and Apple Silicon) and Windows 10/11. Free Demo, no time limit.', body)

# ----------------------------------------------------------------------------------------------- support
def support():
    unlock = shot('unlock'); locked = shot('locked')
    body = f'''
<section class="pagehead"><div class="wrap"><div class="eyebrow">Support</div><h1>Help, keys and answers.</h1><p class="lede">Most answers are below. Anything else: <a href="mailto:support@squelchmoth.com">support@squelchmoth.com</a> — every email gets a personal reply.</p></div></section>
<section class="band tight" id="activate"><div class="wrap split">
  <div><div class="eyebrow">Activate a key</div><h2>Paste it once. Works offline.</h2>
    <ol class="steps"><li>Find the key in your purchase email. It starts with <code>SQM2-</code>.</li><li>Open Squelchmoth, choose <strong>Help › Licences and Upgrades</strong>.</li><li>Paste the key and press <strong>Activate</strong>. The product's card turns to <em>Owned</em>.</li><li>If a session already had locked plug-ins, press <strong>Reopen session to unlock</strong>.</li></ol>
    <p>Bought more than one product? Enter each key — they stack. Keys are checked on your computer with a digital signature; nothing is sent anywhere.</p></div>
  {figure(unlock, 'The Licences and Upgrades window', 'Help › Licences and Upgrades: what you own, every product, and the key field.')}
</div></section>
<section class="band alt"><div class="wrap">
  <div class="grid3">
    <div class="card"><div class="k">Lost your key?</div><p>Search your inbox for “Squelchmoth” or “Payhip”. Payhip also lets you re-open any past purchase from the link in the receipt. Still stuck? Email us the address you bought with.</p></div>
    <div class="card"><div class="k">New computer</div><p>Install, paste the same key. Each licence covers two computers, Mac or Windows, used by one person at a time.</p></div>
    <div class="card"><div class="k">“Can't be opened”</div><p>On a Mac, right-click the installer and choose Open, then Open again. On Windows, click More info › Run anyway. If macOS still refuses, open System Settings › Privacy &amp; Security and click Open Anyway.</p></div>
    <div class="card"><div class="k">A plug-in crashed</div><p>Squelchmoth reopens your session with that plug-in switched off. Hold Shift while launching for Safe Mode. Help › Copy diagnostics gives us everything we need.</p></div>
    <div class="card"><div class="k">Locked panel in a session</div><p>The session uses a plug-in from a product you don't own yet. Its settings are kept. Unlock the product and reopen the session to hear it.</p>{figure(locked, 'A locked plug-in panel', cls='shot mini-shot')}</div>
    <div class="card"><div class="k">Refunds</div><p>If something you bought does not work on your computer, email within 30 days for a full refund. <a href="legal.html#refunds">Refund policy</a></p></div>
  </div>
</div></section>
<section class="band" id="faq"><div class="wrap"><header class="sh"><h2>Frequently asked</h2></header>{faq(FAQ_FULL)}</div></section>'''
    return page('support.html', 'Support — Squelchmoth', 'Install help, key activation, refunds and FAQ for Squelchmoth.', body)

# ----------------------------------------------------------------------------------------------- legal
def legal():
    def txt(path):
        s = open(os.path.join(ROOT, path)).read().strip()
        out, para = [], []
        for line in s.split('\n'):
            if not line.strip():
                if para: out.append(para); para = []
            else: para.append(line)
        if para: out.append(para)
        h = ''
        for blk in out:
            first = blk[0].strip()
            if len(blk) == 1 and first.isupper() and len(first) < 90: h += f'<h3>{E(first)}</h3>'
            elif re.match(r'^(\d+\.|[A-Z][A-Z ,\'-]{3,})\s', first) and first.split(' ')[0].rstrip('.').isdigit():
                h += f'<h3>{E(first)}</h3>' + (f'<p>{E(" ".join(l.strip() for l in blk[1:]))}</p>' if blk[1:] else '')
            elif first.isupper() and len(first) < 60:
                h += f'<h3>{E(first)}</h3>' + (f'<p>{E(" ".join(l.strip() for l in blk[1:]))}</p>' if blk[1:] else '')
            else: h += f'<p>{E(" ".join(l.strip() for l in blk))}</p>'
        return h
    body = f'''
<section class="pagehead"><div class="wrap"><div class="eyebrow">Legal</div><h1>Licence, refunds and privacy.</h1>
  <nav class="jump"><a href="#eula">Licence agreement</a><a href="#refunds">Refund policy</a><a href="#privacy">Privacy policy</a><a href="#notices">Third-party notices</a></nav></div></section>
<section class="band tight legal"><div class="wrap narrow">
  <article id="eula">{txt('scripts/pkg/license.txt')}</article>
  <article id="refunds"><h2>Refund policy</h2>
    <p>Please try the free Demo before buying — it runs the same program on your computer.</p>
    <p>If a product you bought does not install or run on your computer, email <a href="mailto:support@squelchmoth.com">support@squelchmoth.com</a> within 30 days of purchase with your order email. We will help, and if we can't fix it you get a full refund through Payhip, the store that processed your payment. A refund cancels that product's licence key.</p>
    <p>Because keys are delivered instantly and work offline, we don't refund for change of mind once a key has been used. This does not affect any rights you have under consumer law where you live.</p></article>
  <article id="privacy">{txt('docs/legal/PRIVACY.txt')}</article>
  <article id="notices">{txt('docs/legal/THIRD-PARTY-NOTICES.txt')}</article>
</div></section>'''
    return page('legal.html', 'Legal — Squelchmoth', 'Squelchmoth licence agreement, refund policy, privacy policy and third-party notices.', body)

# ----------------------------------------------------------------------------------------------- write
CSS = open(os.path.join(ROOT, 'scripts/site/style.css')).read()
open(os.path.join(SITE, 'style.css'), 'w').write(CSS)
store_js = os.path.join(SITE, 'store.js')
if not os.path.exists(store_js) or '--fresh-store' in sys.argv:
    shutil.copy(os.path.join(ROOT, 'scripts/site/store.js'), store_js)
if not os.path.exists(os.path.join(SITE, 'img/logo.png')):
    im = Image.open(os.path.join(ROOT, 'Resources/icon_1024.png')); im.thumbnail((256, 256)); im.save(os.path.join(SITE, 'img/logo.png'))
for name, fn in [('index.html', index), ('compare.html', compare), ('addons.html', addons), ('download.html', download), ('support.html', support), ('legal.html', legal)]:
    open(os.path.join(SITE, name), 'w').write(fn())
print(f'site built: {N_SIG} Signature ({N_DEMO_SIG} demo, {N_CORE_SIG} core), packs {[ (c, len(by(c))) for c in PACK_CODE.values()]}, complete saves ${SAVE:.2f}')
