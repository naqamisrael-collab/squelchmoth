#!/usr/bin/env python3
"""Reads an in-app preset audit report (Squelchmoth --preset-audit report.md --only "Hive Rack") and updates
scripts/hive_trims.json: each chain's Output trim moves by minus the level it measured on its own material (the starred
column), with the peak kept under +3 dBFS. Run gen_presets_111f.py afterwards to write the presets."""
import json, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tp = os.path.join(ROOT, 'scripts', 'hive_trims.json')
trims = json.load(open(tp)) if os.path.exists(tp) else {}
n = 0
for line in open(sys.argv[1]):
    if not line.startswith('| Hive Rack |'): continue
    cells = [c.strip() for c in line.strip().strip('|').split('|')]
    name = cells[1].replace(' / ', ' | ', 1); lv = cells[2:5]; peak = float(cells[5])
    cat = name.split(' | ')[0]
    t = 2 if ('vocal' in cat.lower() or 'voice' in cat.lower()) else (1 if ('drum' in cat.lower() or cat.startswith(('808', 'Dub', 'Rhythmic', 'Snare'))) else 0)
    level = float(lv[t].rstrip('*'))
    old = trims.get(name, 0.0)
    new = old - level
    new = min(new, old + (3.0 - peak))
    trims[name] = round(max(-24.0, min(12.0, new)), 2); n += 1
json.dump(trims, open(tp, 'w'), indent=1, sort_keys=True)
print('updated', n, 'chain trims; largest', max(abs(v) for v in trims.values()) if trims else 0)
