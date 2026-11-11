#!/usr/bin/env python3
"""Payhip product covers (1280x720 PNG) for the 9 store products, rendered from the site's own art and screenshots.
    python3 scripts/make_covers.py <out_dir>
"""
import os, sys, subprocess, json
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, 'site')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'dist/covers')
os.makedirs(OUT, exist_ok=True)

C = [  # code, title, line, price, list, image, accent
    ('DEMO', 'Squelchmoth Demo', 'Free forever · Mac and Windows · 16 tracks · 28 instruments and effects · five kits', 'FREE', '', 'img/shots/edit.jpg', '#3affb0'),
    ('CORE', 'Squelchmoth', 'The full program for Mac and Windows · unlimited tracks · 60 Signature plug-ins + 186 AirWindows · Mastering · Cut Lab', '$189.99', '$299.99', 'img/shots/edit.jpg', '#d4ff3a'),
    ('COMPLETE', 'Squelchmoth Complete', 'The program and all six add-on packs · 123 Signature plug-ins · 5,300+ presets · 27 kits · 51 loops', '$399.99', '$699.99', 'img/shots/mastering.jpg', '#b06bff'),
    ('VOCAL', 'Vocal Suite', '17 vocal plug-ins · Moth Tune · Hive Choir · Vox Moth · Talk Wing · Clean Room · and more', '$149', '$249', 'img/shots/voxmoth.jpg', '#19e3ff'),
    ('COLOR', 'Color & Space', '16 plug-ins · Cinder · Wingbeat Echo · Foil Plate · Hive Rack · and more · 3,000+ presets', '$179', '$299', 'img/shots/cinder.jpg', '#ff6a3a'),
    ('MASTER', 'Mastering Suite', 'The Apex series · Pulse EQ · Opto Leveler', '$99', '$149', 'img/shots/apexlim.jpg', '#ff5a3a'),
    ('INSTR', 'Instrument Collection', 'Hexwing · Toxin FM · Lantern · Lantern Exo · Pollen · Blossom 2 · and four more', '$99', '$149', 'img/shots/hexwing.jpg', '#3ad7ff'),
    ('BEAT', 'Beat Lab Expansion', 'Carapace · Live Drum Synth · Mandible · ten more kits · 51 genre loops', '$49', '$79', 'img/shots/carapace.jpg', '#ff9a3a'),
    ('CREATIVE', 'Creative FX', 'Time Weaver · Larva Filter · Oil Can Delay · Bucket Brigade · and three more', '$49', '$79', 'img/shots/timeweaver.jpg', '#3affb0'),
]
TPL = '''<!doctype html><html><head><meta charset="utf-8">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Syne:wght@700;800&family=IBM+Plex+Sans:wght@500&family=IBM+Plex+Mono:wght@500&display=swap">
<style>
html,body{{margin:0;width:1280px;height:720px;overflow:hidden;background:#0d0714;font-family:'IBM Plex Sans',sans-serif;color:#efeaff}}
.bg{{position:absolute;inset:0 0 0 38%;background:url("{img}") left top/cover;transform:perspective(1400px) rotateY(-8deg);transform-origin:left center;border-radius:18px 0 0 18px;box-shadow:-40px 0 90px rgba(0,0,0,.7);opacity:.95}}
.fade{{position:absolute;inset:0;background:linear-gradient(90deg,#0d0714 36%,rgba(13,7,20,.75) 52%,rgba(13,7,20,0) 75%)}}
.glow{{position:absolute;left:-200px;top:-260px;width:900px;height:900px;background:radial-gradient(closest-side,{accent}33,transparent)}}
.c{{position:absolute;left:72px;top:0;bottom:0;width:560px;display:flex;flex-direction:column;justify-content:center;gap:20px}}
.brand{{display:flex;align-items:center;gap:12px;font-family:Syne;font-weight:800;letter-spacing:.12em;font-size:20px}}
.brand img{{width:44px;height:44px;border-radius:11px}}
h1{{font-family:Syne;font-weight:800;font-size:{fs}px;line-height:1;margin:0;letter-spacing:-.02em}}
p{{margin:0;font-size:22px;line-height:1.4;color:#c9c0e2}}
.price{{display:flex;align-items:baseline;gap:14px}}
.price b{{font-family:Syne;font-size:58px;color:{accent}}}
.price s{{font-size:26px;color:#8f84aa}}
.tag{{font-family:'IBM Plex Mono';font-size:15px;letter-spacing:.14em;text-transform:uppercase;color:{accent}}}
.bar{{position:absolute;left:0;right:0;bottom:0;height:6px;background:{accent}}}
</style></head><body><div class="glow"></div><div class="bg"></div><div class="fade"></div>
<div class="c"><div class="brand"><img src="img/logo.png">SQUELCHMOTH</div><div class="tag">{tag}</div><h1>{title}</h1><p>{line}</p>
<div class="price"><b>{price}</b><s>{lst}</s></div></div><div class="bar"></div></body></html>'''
pages = []
for code, title, line, price, lst, img, accent in C:
    tag = 'Mac & Windows · one key works on both' if code in ('DEMO', 'CORE', 'COMPLETE') else 'Add-on pack · works in every edition'
    f = os.path.join(SITE, f'_cover_{code}.html')
    open(f, 'w').write(TPL.format(img=img, accent=accent, title=title, line=line, price=price, lst=lst, tag=tag, fs=78 if len(title) < 16 else 62))
    pages.append((f, os.path.join(OUT, f'{code} cover.png')))
js = '''const { chromium } = require('playwright');
(async () => { const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1280, height: 720 } });
for (const [i, o] of %s) { await p.goto('file://' + i); await p.waitForTimeout(900); await p.screenshot({ path: o }); console.log(o); }
await b.close(); })();''' % json.dumps(pages)
open('/tmp/_covers.js', 'w').write(js)
subprocess.run(['node', '/tmp/_covers.js'], check=True)
for f, _ in pages: os.remove(f)
