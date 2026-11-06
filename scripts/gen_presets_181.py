#!/usr/bin/env python3
"""Author the factory preset banks for the 1.8.1 instruments as data and emit Source/SignaturePresets181.inl.

Same format as gen_presets_18.py: "Category | Name", only the parameters that differ from the defaults, props for the
SynthCommon properties (polyMode, glideMode, unison, maxPoly, bendRange, quality), mods as (source, dest, depth).
Level trims measured by the preset sweep live in scripts/preset_levels_181.json."""
import os, json

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Source', 'SignaturePresets181.inl')
ENV1, ENV2, ENV3, LFO1, LFO2, VEL, KEY, WHEEL, AT, MODX, MODY, M1, M2, M3, M4, RND = range(1, 17)
POLY, MONO, LEGATO = 0, 1, 2
SINE, SOFT, SHARP, SQUARE, HALF, CUBED = range(6)            # FM operator waves
LP24, LP12, HP24, HP12, BP, NOTCH, LADDER = range(7)
W_SINE, W_TRI, W_SAW, W_SQR, W_PULSE, W_NOISE, W_TABLE = range(7)
MEMBRANE, METAL, NOISE_M, WOOD, TOM = range(5)
D_SINE, D_TRI, D_SAW, D_SQR, D_NOISE = range(5)

banks = {}
def preset(bank, cat, name, adv=False, props=None, mods=None, **vals):
    banks.setdefault(bank, []).append((cat, name, adv, vals, props or {}, mods or []))

# ------------------------------------------------------------------------------------------------ FM helpers (Hexwing / Toxin)
def op(n, wave=SINE, ratio=1.0, level=None, fb=0.0, a=0.004, d=0.6, s=0.3, r=0.4, fine=0, vel=0.5, key=0.2):
    o = {f'op{n}Wave': wave, f'op{n}Ratio': ratio, f'op{n}Fb': fb, f'op{n}A': a, f'op{n}D': d, f'op{n}S': s, f'op{n}R': r, f'op{n}Vel': vel, f'op{n}Key': key}
    if fine: o[f'op{n}Fine'] = fine
    if level is not None: o[f'op{n}Level'] = level
    return o
def ops(*defs):
    out = {}
    for d in defs: out.update(d)
    return out
def silent(*ns):
    return {f'op{n}Level': 0 for n in ns}

HX = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Hexwing', cat, name, adv, props, mods, **k)
# Keys (algorithm 1: two pairs 2>1 and 6>5>4>3 -> use pairs 2>1, 4>3 on alg 2 "three pairs")
HX('Keys', 'Init Hexwing', algorithm=0, **ops(op(1, level=1, s=0.7, d=1.5), op(2, level=0.5)), **silent(3, 4, 5, 6))
HX('Keys', 'Tine Piano', algorithm=2, **ops(op(1, level=1, d=2.5, s=0.2, r=0.5, vel=0.7), op(4, ratio=14, level=0.22, d=0.25, s=0, r=0.2, vel=0.9), op(2, ratio=1, level=0.7, d=2.2, s=0.15, r=0.5), op(5, ratio=1, level=0.35, d=1.2, s=0, fb=0.2), op(3, ratio=2, level=0.3, d=1.5, s=0.1), op(6, ratio=3, level=0.2, d=0.4, s=0)), fxChorus=0.2, fxReverb=0.18, level=-6)
HX('Keys', 'Glass Piano', algorithm=2, **ops(op(1, level=1, d=3, s=0.1, r=0.8), op(4, ratio=7, level=0.25, d=0.6, s=0), op(2, ratio=2, level=0.5, d=2.5, s=0.05), op(5, ratio=5, level=0.2, d=0.5, s=0), op(3, ratio=1, fine=4, level=0.4, d=3), op(6, ratio=1, level=0.1, d=1)), fxReverb=0.3, fxSize=0.7, level=-7)
HX('Keys', 'Dirty Keys', algorithm=0, **ops(op(1, level=1, d=1.8, s=0.3, r=0.4, vel=0.8), op(2, ratio=1, level=0.55, d=0.8, s=0.2, fb=0.35, vel=0.9), op(3, ratio=2, level=0.3, d=0.3, s=0)), **silent(4, 5, 6), drive=0.25, driveType=1, level=-6)
HX('Keys', 'Clav FM', algorithm=1, **ops(op(1, level=1, d=0.8, s=0.1, r=0.1, vel=0.9), op(2, ratio=3, level=0.5, d=0.15, s=0.15, vel=1), op(3, ratio=1, level=0.6, d=0.6, s=0.1, r=0.1), op(4, ratio=7, level=0.3, d=0.08, s=0.05), op(5, ratio=1, level=0.3, d=0.1, s=0.3), op(6, ratio=2, level=0.2, d=0.1, s=0.2)), level=-6)
HX('Keys', 'Wurli Grit', algorithm=0, **ops(op(1, level=1, d=2.2, s=0.25, r=0.4, vel=0.8), op(2, wave=SOFT, ratio=1, level=0.5, d=1.0, s=0.3, fb=0.15, vel=0.9), op(3, ratio=4, level=0.15, d=0.2, s=0)), **silent(4, 5, 6), fType=LP12, cutoff=5000, level=-6)
HX('Keys', 'Harpsichord', algorithm=3, **ops(op(1, level=1, d=1.2, s=0, r=0.15, vel=0.6), op(2, ratio=2, level=0.4, d=0.9, s=0), op(3, ratio=5, level=0.3, d=0.5, s=0), op(4, ratio=8, level=0.25, d=0.3, s=0), op(5, ratio=1, level=0.2, d=0.3, s=0), op(6, ratio=1, level=0.2, d=0.3, s=0)), level=-6)
HX('Keys', 'Celesta', algorithm=2, **ops(op(1, level=1, d=2.0, s=0, r=0.6), op(4, ratio=4, level=0.2, d=0.4, s=0), op(2, ratio=4, level=0.4, d=1.2, s=0), op(5, ratio=1, level=0.1, d=0.5, s=0), op(3, ratio=1, level=0.3, d=1.6, s=0), op(6, ratio=1, level=0, d=0.5, s=0)), fxReverb=0.25, level=-7)
# Bells & Mallets
HX('Bells', 'Tubular Bell', algorithm=2, **ops(op(1, level=1, d=5, s=0, r=3), op(4, ratio=3.5, level=0.45, d=3, s=0), op(2, ratio=2.01, level=0.5, d=4, s=0), op(5, ratio=5.19, level=0.3, d=2, s=0), op(3, ratio=1, fine=6, level=0.4, d=5, s=0), op(6, ratio=7.07, level=0.2, d=1, s=0)), fxReverb=0.4, fxSize=0.8, level=-8)
HX('Bells', 'Glass Bell', algorithm=5, **ops(op(1, level=1, d=3, s=0, r=2), op(2, ratio=7, level=0.3, d=1.2, s=0), op(3, ratio=2, level=0.6, d=2.5, s=0), op(4, ratio=11, level=0.2, d=0.6, s=0), op(5, ratio=4.5, level=0.3, d=2, s=0), op(6, ratio=1, level=0.15, d=1, s=0)), fxReverb=0.35, level=-8)
HX('Bells', 'Music Box', algorithm=1, **ops(op(1, level=1, d=1.5, s=0, r=0.8), op(2, ratio=4, level=0.3, d=0.5, s=0), op(3, ratio=3, level=0.5, d=1.2, s=0), op(4, ratio=1, level=0.2, d=0.3, s=0), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), fxReverb=0.3, level=-7)
HX('Bells', 'Gamelan', algorithm=2, **ops(op(1, level=1, d=2.5, s=0, r=1.5), op(4, ratio=2.76, level=0.5, d=1.5, s=0), op(2, ratio=1.5, level=0.6, d=2, s=0), op(5, ratio=4.1, level=0.35, d=1.0, s=0), op(3, ratio=3.2, level=0.3, d=1.8, s=0), op(6, ratio=1.31, level=0.3, d=0.8, s=0)), level=-8)
HX('Bells', 'Marimba', algorithm=1, **ops(op(1, level=1, d=0.9, s=0, r=0.4, vel=0.8), op(2, ratio=4, level=0.3, d=0.12, s=0, vel=1), op(3, ratio=1, level=0.7, d=0.6, s=0, r=0.3), op(4, ratio=10, level=0.25, d=0.06, s=0), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), level=-6)
HX('Bells', 'Vibraphone', algorithm=1, **ops(op(1, level=1, d=3, s=0, r=1.5), op(2, ratio=4, level=0.2, d=1.0, s=0), op(3, ratio=1, level=0.6, d=3, s=0, r=1.5), op(4, ratio=3, level=0.25, d=0.6, s=0), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), lfo1Rate=5.5, mods=[(LFO1, 'amp', 0.25)], fxReverb=0.3, level=-7)
HX('Bells', 'Kalimba', algorithm=1, **ops(op(1, level=1, d=1.2, s=0, r=0.5, vel=0.8), op(2, ratio=6, level=0.35, d=0.1, s=0), op(3, ratio=2, level=0.5, d=0.8, s=0), op(4, ratio=1, level=0.2, d=0.2, s=0), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), level=-6)
# Bass
HX('Bass', 'FM Bass Classic', algorithm=0, **ops(op(1, level=1, d=1.0, s=0.6, r=0.1, vel=0.6), op(2, ratio=1, level=0.6, d=0.35, s=0.2, fb=0.3, vel=0.9), op(3, ratio=2, level=0.35, d=0.2, s=0)), **silent(4, 5, 6), props={'polyMode': MONO}, level=-5)
HX('Bass', 'Late Night Bass', algorithm=0, **ops(op(1, level=1, d=0.8, s=0.5, r=0.1, vel=0.7), op(2, ratio=1, level=0.7, d=0.25, s=0.3, fb=0.45, vel=1), op(3, ratio=1, level=0.4, d=0.15, s=0.1)), **silent(4, 5, 6), props={'polyMode': MONO}, level=-5)
HX('Bass', 'Solid Sub', algorithm=0, **ops(op(1, level=1, d=1.5, s=0.9, r=0.15), op(2, ratio=1, level=0.25, d=0.3, s=0.1)), **silent(3, 4, 5, 6), fType=LP24, cutoff=600, props={'polyMode': MONO, 'glideMode': 2}, glide=0.06, level=-3)
HX('Bass', 'Wood Bass', algorithm=0, **ops(op(1, level=1, d=0.9, s=0.3, r=0.12, vel=0.8), op(2, ratio=2, level=0.4, d=0.18, s=0.1, vel=1), op(3, ratio=0.5, level=0.3, d=0.4, s=0.3)), **silent(4, 5, 6), props={'polyMode': MONO}, level=-5)
HX('Bass', 'Slap FM', algorithm=1, **ops(op(1, level=1, d=0.6, s=0.4, r=0.1, vel=0.9), op(2, ratio=4, level=0.55, d=0.08, s=0.1, vel=1), op(3, ratio=1, level=0.5, d=0.5, s=0.4), op(4, ratio=1, level=0.3, d=0.2, s=0.2, fb=0.3), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), props={'polyMode': MONO}, level=-5)
HX('Bass', 'Growl Bass', True, algorithm=0, **ops(op(1, level=1, d=1, s=0.8, r=0.1), op(2, wave=SHARP, ratio=1, level=0.6, d=0.5, s=0.5, fb=0.5), op(3, ratio=0.5, level=0.4, d=0.3, s=0.4)), **silent(4, 5, 6), lfo1Rate=0.5, lfo1Sync=1, mods=[(LFO1, 'index', 0.5), (M1, 'index', 0.6)], drive=0.4, driveType=1, props={'polyMode': MONO}, level=-7)
HX('Bass', 'Reese FM', True, algorithm=7, **ops(op(1, wave=SHARP, level=0.8, d=1, s=1, r=0.2), op(2, wave=SHARP, ratio=1, fine=12, level=0.8, d=1, s=1, r=0.2), op(3, wave=SHARP, ratio=1, fine=-12, level=0.8, d=1, s=1, r=0.2), op(4, ratio=0.5, level=0.6, d=1, s=1, r=0.2)), **silent(5, 6), fType=LP24, cutoff=900, res=0.2, props={'polyMode': MONO, 'unison': 2}, uniDetune=12, level=-8)
# Brass / Lead
HX('Brass', 'FM Brass', algorithm=0, **ops(op(1, level=1, a=0.04, d=0.5, s=0.8, r=0.2), op(2, ratio=1, level=0.55, a=0.08, d=0.4, s=0.6, fb=0.2, vel=0.7), op(3, ratio=1, level=0.3, a=0.12, d=0.3, s=0.5)), **silent(4, 5, 6), lfo1Rate=5.2, mods=[(LFO1, 'pitch', 0.01), (WHEEL, 'index', 0.4)], props={'unison': 2}, uniDetune=8, level=-7)
HX('Brass', 'Synth Horns', algorithm=5, **ops(op(1, level=1, a=0.05, d=0.4, s=0.8, r=0.25), op(2, ratio=1, level=0.45, a=0.1, s=0.6), op(3, level=0.9, a=0.06, s=0.8, fine=5), op(4, ratio=2, level=0.3, a=0.1, s=0.5), op(5, level=0.9, a=0.07, s=0.8, fine=-5), op(6, ratio=1, level=0.4, a=0.1, s=0.6)), fxChorus=0.3, level=-9)
HX('Lead', 'Sine Lead', algorithm=0, **ops(op(1, level=1, a=0.01, s=1, r=0.2), op(2, ratio=1, level=0.25, s=0.5)), **silent(3, 4, 5, 6), lfo1Rate=5.5, mods=[(WHEEL, 'pitch', 0.03), (LFO1, 'pitch', 0.01)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.06, fxDelay=0.25, fxDelayTime=0.75, level=-6)
HX('Lead', 'Fat FM Lead', algorithm=1, **ops(op(1, level=1, a=0.005, s=0.9, r=0.25), op(2, ratio=1, level=0.6, s=0.6, fb=0.25), op(3, level=1, fine=8, s=0.9, r=0.25), op(4, ratio=2, level=0.4, s=0.5), op(5, ratio=1, level=0.2, s=0.4), op(6, ratio=1, level=0, s=0)), props={'unison': 3}, uniDetune=14, drive=0.2, fxChorus=0.2, level=-9)
HX('Lead', 'Whistle', algorithm=0, **ops(op(1, level=1, a=0.08, s=1, r=0.3), op(2, ratio=2, level=0.12, s=0.3)), **silent(3, 4, 5, 6), lfo1Rate=6, mods=[(LFO1, 'pitch', 0.015)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.08, fxReverb=0.3, level=-7)
HX('Lead', 'Square Lead FM', algorithm=0, **ops(op(1, wave=SQUARE, level=1, s=0.9, r=0.2), op(2, ratio=1, level=0.3, s=0.6)), **silent(3, 4, 5, 6), fType=LP12, cutoff=4000, props={'polyMode': MONO, 'glideMode': 2}, glide=0.05, level=-7)
# Pads
HX('Pad', 'FM Strings', algorithm=5, **ops(op(1, level=1, a=0.6, s=1, r=1.5), op(2, ratio=1, level=0.35, a=0.8, s=0.7), op(3, level=0.9, fine=7, a=0.7, s=1, r=1.5), op(4, ratio=2, level=0.25, a=0.9, s=0.6), op(5, level=0.9, fine=-7, a=0.8, s=1, r=1.5), op(6, ratio=3, level=0.2, a=1.0, s=0.5)), fxChorus=0.4, fxReverb=0.4, fxSize=0.8, level=-10)
HX('Pad', 'Glass Pad', algorithm=2, **ops(op(1, level=1, a=1.2, s=1, r=2.5), op(4, ratio=3, level=0.3, a=2.0, s=0.6), op(2, ratio=2, level=0.6, a=1.5, s=1, r=2.5), op(5, ratio=5, level=0.2, a=2.5, s=0.5), op(3, ratio=1, fine=5, level=0.6, a=1.0, s=1, r=2.5), op(6, ratio=7, level=0.15, a=3, s=0.4)), lfo1Rate=0.2, mods=[(LFO1, 'index', 0.3)], fxReverb=0.5, fxSize=0.9, level=-10)
HX('Pad', 'Evolving Metal', True, algorithm=3, **ops(op(1, level=1, a=1.5, s=1, r=3), op(2, ratio=1.41, level=0.4, a=3, s=0.7), op(3, ratio=2.3, level=0.35, a=4, s=0.6), op(4, ratio=0.5, level=0.4, a=2, s=0.8), op(5, ratio=3, level=0.3, a=5, s=0.5), op(6, ratio=7, level=0.2, a=6, s=0.4)), lfo1Rate=0.07, lfo2Rate=0.11, mods=[(LFO1, 'index', 0.5), (LFO2, 'pan', 0.6)], fxReverb=0.6, fxSize=0.9, fxDelay=0.3, fxDelayTime=1.5, fxDelayFb=0.5, level=-11)
HX('Pad', 'Warm Choir', algorithm=7, **ops(op(1, level=1, a=0.9, s=1, r=2), op(2, ratio=2, level=0.35, a=1.2, s=1, r=2), op(3, ratio=3, level=0.2, a=1.5, s=1, r=2), op(4, ratio=1, fine=6, level=0.8, a=1.0, s=1, r=2), op(5, ratio=4, level=0.1, a=2, s=1, r=2)), **silent(6), fType=BP, cutoff=1200, res=0.3, lfo1Rate=0.15, mods=[(LFO1, 'cutoff', 0.4)], fxChorus=0.5, fxReverb=0.5, level=-10)
HX('Pad', 'Dark Drone', True, algorithm=0, **ops(op(1, level=1, a=2, s=1, r=4), op(2, wave=SOFT, ratio=0.5, level=0.5, a=3, s=1, fb=0.4), op(3, ratio=0.25, level=0.4, a=4, s=1)), **silent(4, 5, 6), fType=LP24, cutoff=1500, lfo1Rate=0.05, mods=[(LFO1, 'index', 0.6), (LFO1, 'cutoff', 0.3)], fxReverb=0.7, fxSize=0.95, level=-10)
# Organ / Pluck / FX
HX('Organ', 'Drawbar Organ', algorithm=7, **ops(op(1, level=1, a=0.003, s=1, r=0.04), op(2, ratio=2, level=0.7, a=0.003, s=1, r=0.04), op(3, ratio=3, level=0.4, a=0.003, s=1, r=0.04), op(4, ratio=4, level=0.3, a=0.003, s=1, r=0.04), op(5, ratio=0.5, level=0.6, a=0.003, s=1, r=0.04), op(6, ratio=6, level=0.15, a=0.003, s=1, r=0.04)), lfo1Rate=6.5, mods=[(LFO1, 'amp', 0.15)], fxChorus=0.35, level=-9)
HX('Organ', 'Percussive Organ', algorithm=7, **ops(op(1, level=1, a=0.003, s=1, r=0.05), op(2, ratio=2, level=0.5, a=0.003, s=1, r=0.05), op(3, ratio=3, level=0.6, a=0.003, d=0.3, s=0, r=0.05), op(4, ratio=0.5, level=0.5, a=0.003, s=1, r=0.05)), **silent(5, 6), drive=0.15, level=-8)
HX('Pluck', 'FM Pluck', algorithm=0, **ops(op(1, level=1, d=0.7, s=0, r=0.3, vel=0.7), op(2, ratio=2, level=0.5, d=0.25, s=0, vel=1), op(3, ratio=3, level=0.3, d=0.12, s=0)), **silent(4, 5, 6), fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6)
HX('Pluck', 'Koto', algorithm=1, **ops(op(1, level=1, d=1.0, s=0, r=0.4, vel=0.8), op(2, ratio=3, level=0.45, d=0.15, s=0, vel=1), op(3, ratio=1, fine=4, level=0.6, d=0.9, s=0), op(4, ratio=5, level=0.3, d=0.1, s=0), op(5, ratio=1, level=0, d=0.3, s=0), op(6, ratio=1, level=0, d=0.3, s=0)), level=-6)
HX('Pluck', 'Steel Drum', algorithm=2, **ops(op(1, level=1, d=1.2, s=0, r=0.6), op(4, ratio=2, level=0.5, d=0.3, s=0), op(2, ratio=2, level=0.5, d=1, s=0), op(5, ratio=4, level=0.3, d=0.2, s=0), op(3, ratio=3, level=0.3, d=0.8, s=0), op(6, ratio=1, level=0.25, d=0.3, s=0)), fxReverb=0.3, level=-7)
HX('Pluck', 'Harp FM', algorithm=0, **ops(op(1, level=1, d=1.8, s=0, r=0.8, vel=0.6), op(2, ratio=2, level=0.35, d=0.4, s=0), op(3, ratio=1, level=0.2, d=0.2, s=0)), **silent(4, 5, 6), fxReverb=0.35, fxSize=0.7, level=-6)
HX('FX', 'Metal Hit', True, algorithm=3, **ops(op(1, level=1, d=1.5, s=0, r=0.5), op(2, ratio=1.41, level=0.7, d=0.8, s=0, fb=0.5), op(3, ratio=3.7, level=0.6, d=0.5, s=0), op(4, ratio=5.3, level=0.5, d=0.3, s=0), op(5, ratio=11, level=0.4, d=0.2, s=0), op(6, ratio=13, level=0.3, d=0.1, s=0)), fxReverb=0.4, level=-8)
HX('FX', 'Laser Sweep', True, algorithm=0, **ops(op(1, level=1, d=0.4, s=0, r=0.1), op(2, ratio=1, level=0.8, d=0.3, s=0, fb=0.6), op(3, ratio=4, level=0.5, d=0.1, s=0)), **silent(4, 5, 6), mods=[(ENV2, 'pitch', 1.0)], fA=0.001, fD=0.3, fS=0, props={'polyMode': MONO}, level=-6)
HX('FX', 'Alien Voice', True, algorithm=6, **ops(op(1, level=1, s=1), op(2, ratio=2, level=0.7, s=1), op(3, ratio=3, level=0.5, s=1), op(4, ratio=5, level=0.4, s=1), op(5, ratio=7, level=0.3, s=1), op(6, wave=SOFT, ratio=0.33, level=0.5, s=1, fb=0.3)), lfo1Rate=4, lfo2Rate=0.3, mods=[(LFO1, 'index', 0.4), (LFO2, 'op6', 0.6)], fType=BP, cutoff=1000, res=0.5, level=-8)
HX('FX', 'Noise Wash', True, algorithm=0, **ops(op(1, level=1, a=1.5, s=1, r=2), op(2, wave=SHARP, ratio=7.13, level=0.9, a=1.5, s=1, fb=0.8), op(3, ratio=11.7, level=0.8, s=1, fb=0.7)), **silent(4, 5, 6), fType=BP, cutoff=2000, res=0.4, lfo1Rate=0.2, mods=[(LFO1, 'cutoff', 0.6)], fxReverb=0.6, fxSize=0.9, level=-10)
HX('FX', 'Riser FM', True, algorithm=0, **ops(op(1, level=1, a=3, s=1, r=0.5), op(2, ratio=1, level=0.6, a=4, s=1, fb=0.5), op(3, ratio=2, level=0.4, a=4, s=1)), **silent(4, 5, 6), mods=[(ENV2, 'pitch', 0.5), (ENV2, 'index', 0.5)], fA=4, fS=1, props={'unison': 3}, uniDetune=25, fxReverb=0.5, level=-10)

# ------------------------------------------------------------------------------------------------ Toxin FM
TX = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Toxin FM', cat, name, adv, props, mods, **k)
def seq(notes, on=None, acc=None, slide=None, rate=1, swing=0, gate=0.6, octave=0):
    d = {'seqOn': 1, 'seqRate': rate, 'seqSwing': swing, 'seqGate': gate, 'seqOctave': octave}
    for i in range(16):
        d[f'step{i+1}Note'] = notes[i % len(notes)]
        d[f'step{i+1}On'] = 1 if on is None else on[i % len(on)]
        d[f'step{i+1}Acc'] = 0 if acc is None else acc[i % len(acc)]
        d[f'step{i+1}Slide'] = 0 if slide is None else slide[i % len(slide)]
    return d
ACID1 = seq([0, 0, 12, 0, 3, 0, 0, 12, 0, 0, 7, 0, 5, 3, 0, 12], on=[1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1], acc=[1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1], slide=[0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0])
ACID2 = seq([0, 12, 0, 0, 0, 12, 10, 0, 0, 12, 0, 0, 7, 0, 12, 0], on=[1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1], acc=[1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0], slide=[0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0])
OCT = seq([0, 12] * 8, acc=[1, 0, 0, 0] * 4)
ARP = seq([0, 4, 7, 12, 7, 4, 0, -5, 0, 4, 7, 12, 16, 12, 7, 4], acc=[1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
MINOR = seq([0, 3, 7, 12, 10, 7, 3, 0, 0, 3, 7, 15, 12, 7, 3, 0], acc=[1, 0, 0, 1, 0, 0, 1, 0] * 2)
TRANCE = seq([0, 0, 12, 0, 0, 12, 0, 0, 7, 0, 0, 7, 0, 3, 0, 12], on=[1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1], acc=[1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1], gate=0.4)
DRILL = seq([0, 0, 0, -2, 0, 0, 3, 0, 0, 0, 0, -2, 0, 5, 3, 0], on=[1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0], acc=[1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0], slide=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0], rate=1, swing=20)
ACIDOP = ops(op(1, level=1, d=0.5, s=0.6, r=0.08, vel=0.6), op(2, ratio=1, level=0.55, d=0.2, s=0.3, fb=0.4, vel=1), op(3, ratio=2, level=0.3, d=0.12, s=0))
TX('Acid', 'Init Toxin', algorithm=0, **ops(op(1, level=1, s=0.7, d=1.5), op(2, level=0.45)), **silent(3, 4, 5, 6), cutoff=4000)
TX('Acid', 'Acid FM Line', algorithm=0, **ACIDOP, **silent(4, 5, 6), cutoff=700, res=0.6, fEnv=0.7, fD=0.25, fS=0, drive=0.35, driveType=1, **ACID1, props={'polyMode': MONO, 'glideMode': 2}, glide=0.07, level=-6)
TX('Acid', 'Squelch Toxin', algorithm=0, **ACIDOP, **silent(4, 5, 6), cutoff=400, res=0.8, fEnv=0.9, fD=0.3, fS=0.05, drive=0.5, driveType=2, **ACID2, props={'polyMode': MONO, 'glideMode': 2}, glide=0.08, level=-7)
TX('Acid', 'Rubber Acid', algorithm=1, **ops(op(1, level=1, d=0.4, s=0.5, r=0.08), op(2, ratio=1, level=0.4, d=0.3, s=0.3, fb=0.5), op(3, level=0.8, d=0.5, s=0.5, r=0.08), op(4, ratio=2, level=0.35, d=0.15, s=0.1), op(5, ratio=1, level=0.2, d=0.2, s=0.2), op(6, ratio=1, level=0, s=0)), cutoff=600, res=0.7, fEnv=0.8, fD=0.2, fS=0, **ACID1, props={'polyMode': MONO, 'glideMode': 2}, level=-6)
TX('Acid', 'Metallic Acid', True, algorithm=0, **ops(op(1, level=1, d=0.5, s=0.6, r=0.08), op(2, ratio=3.5, level=0.5, d=0.2, s=0.3, fb=0.4), op(3, ratio=1, level=0.4, d=0.3, s=0.2)), **silent(4, 5, 6), cutoff=900, res=0.6, fEnv=0.6, fD=0.22, fS=0, drive=0.4, driveType=3, **ACID2, props={'polyMode': MONO, 'glideMode': 2}, level=-7)
TX('Acid', 'Slow Acid Pad', True, algorithm=0, **ACIDOP, **silent(4, 5, 6), cutoff=500, res=0.5, fEnv=0.7, fD=0.4, fS=0.1, **seq([0, 0, 3, 0, 7, 0, 10, 0], on=[1, 0, 1, 0, 1, 0, 1, 0], rate=0, slide=[1, 0] * 8), props={'polyMode': MONO, 'glideMode': 2}, glide=0.15, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.45, fxReverb=0.3, level=-8)
TX('Acid', 'Triplet Acid', True, algorithm=0, **ACIDOP, **silent(4, 5, 6), cutoff=650, res=0.65, fEnv=0.75, fD=0.2, fS=0, drive=0.3, **seq([0, 0, 12, 0, 3, 12, 0, 7, 12, 0, 0, 12], acc=[1, 0, 0] * 5 + [1], rate=3), props={'polyMode': MONO, 'glideMode': 2}, level=-6)
# Sequenced bass
TX('Seq Bass', 'Octave Pump', algorithm=0, **ops(op(1, level=1, d=0.4, s=0.4, r=0.08, vel=0.7), op(2, ratio=1, level=0.5, d=0.15, s=0.2, vel=1), op(3, ratio=2, level=0.2, d=0.1, s=0)), **silent(4, 5, 6), cutoff=1200, fEnv=0.4, fD=0.15, fS=0, **OCT, props={'polyMode': MONO}, level=-5)
TX('Seq Bass', 'Drill Slide', True, algorithm=0, **ops(op(1, level=1, d=1.2, s=0.6, r=0.15), op(2, ratio=1, level=0.3, d=0.4, s=0.2, fb=0.2)), **silent(3, 4, 5, 6), cutoff=500, fEnv=0.2, **DRILL, drive=0.3, driveType=1, props={'polyMode': MONO, 'glideMode': 2}, glide=0.12, level=-3)
TX('Seq Bass', 'Trance Bass', algorithm=0, **ops(op(1, level=1, d=0.3, s=0.5, r=0.06), op(2, ratio=1, level=0.45, d=0.12, s=0.3), op(3, ratio=2, level=0.25, d=0.1, s=0)), **silent(4, 5, 6), cutoff=800, fEnv=0.5, fD=0.12, fS=0, **TRANCE, props={'polyMode': MONO}, level=-5)
TX('Seq Bass', 'Wood Seq', algorithm=0, **ops(op(1, level=1, d=0.6, s=0.3, r=0.1, vel=0.8), op(2, ratio=2, level=0.4, d=0.15, s=0.1, vel=1), op(3, ratio=0.5, level=0.3, d=0.3, s=0.3)), **silent(4, 5, 6), **MINOR, props={'polyMode': MONO}, level=-5)
TX('Seq Bass', 'Electro Funk', algorithm=1, **ops(op(1, level=1, d=0.35, s=0.4, r=0.06, vel=0.9), op(2, ratio=1, level=0.5, d=0.12, s=0.25, fb=0.3, vel=1), op(3, level=0.7, d=0.4, s=0.3, r=0.06), op(4, ratio=4, level=0.3, d=0.06, s=0.05), op(5, ratio=1, level=0.1, s=0.3), op(6, level=0, s=0)), cutoff=1500, fEnv=0.4, fD=0.1, fS=0, **seq([0, 0, 12, 0, 0, 10, 0, 12, 0, 0, 12, 0, 7, 0, 10, 12], on=[1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1], acc=[1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1], swing=12), props={'polyMode': MONO}, level=-5)
TX('Seq Bass', 'Sub Sequence', algorithm=0, **ops(op(1, level=1, d=0.8, s=0.7, r=0.1), op(2, ratio=1, level=0.2, d=0.3, s=0.1)), **silent(3, 4, 5, 6), cutoff=500, **seq([0, 0, 0, 0, -5, -5, -5, -5, -2, -2, -2, -2, 0, 0, 3, 5], acc=[1, 0, 0, 0] * 4, gate=0.9), props={'polyMode': MONO, 'glideMode': 2}, glide=0.05, level=-3)
# Arps
TX('Arp', 'Pluck Arp', algorithm=0, **ops(op(1, level=1, d=0.5, s=0, r=0.25, vel=0.7), op(2, ratio=2, level=0.45, d=0.2, s=0, vel=1), op(3, ratio=3, level=0.25, d=0.1, s=0)), **silent(4, 5, 6), cutoff=6000, **ARP, fxDelay=0.3, fxDelayTime=0.375, fxDelayFb=0.4, props={'polyMode': MONO}, level=-6)
TX('Arp', 'Bell Arp', algorithm=2, **ops(op(1, level=1, d=1.2, s=0, r=0.5), op(4, ratio=3.5, level=0.35, d=0.4, s=0), op(2, ratio=2, level=0.5, d=1, s=0), op(5, ratio=5, level=0.25, d=0.3, s=0), op(3, ratio=1, fine=5, level=0.4, d=1.2, s=0), op(6, level=0, s=0)), cutoff=12000, **seq([0, 7, 12, 19, 12, 7, 0, 7, 3, 10, 15, 22, 15, 10, 3, 10], acc=[1, 0, 0, 0] * 4, gate=0.5), fxReverb=0.35, fxDelay=0.25, fxDelayTime=0.75, level=-8)
TX('Arp', 'Minor Arp', algorithm=0, **ops(op(1, level=1, d=0.4, s=0.2, r=0.2), op(2, ratio=1, level=0.5, d=0.2, s=0.2, fb=0.2), op(3, ratio=2, level=0.2, d=0.1, s=0)), **silent(4, 5, 6), cutoff=2500, fEnv=0.5, fD=0.15, fS=0, **MINOR, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6)
TX('Arp', 'Trance Arp', algorithm=1, **ops(op(1, level=1, d=0.3, s=0.1, r=0.2), op(2, ratio=1, level=0.5, d=0.15, s=0.1), op(3, level=0.9, fine=8, d=0.3, s=0.1, r=0.2), op(4, ratio=2, level=0.3, d=0.1, s=0), op(5, level=0, s=0), op(6, level=0, s=0)), cutoff=3000, fEnv=0.6, fD=0.12, fS=0, **TRANCE, fxDelay=0.35, fxDelayTime=0.375, fxDelayFb=0.5, fxReverb=0.25, level=-7)
TX('Arp', 'Glass Arp', True, algorithm=5, **ops(op(1, level=1, d=0.8, s=0, r=0.4), op(2, ratio=7, level=0.3, d=0.2, s=0), op(3, ratio=2, level=0.6, d=0.6, s=0), op(4, ratio=11, level=0.2, d=0.1, s=0), op(5, ratio=4.5, level=0.3, d=0.5, s=0), op(6, level=0.15, d=0.3, s=0)), cutoff=12000, **ARP, fxReverb=0.4, fxDelay=0.3, fxDelayTime=0.5, level=-8)
TX('Arp', 'Chip Arp', algorithm=7, **ops(op(1, wave=SQUARE, level=1, d=0.2, s=0.6, r=0.05), op(2, wave=SQUARE, ratio=2, level=0.4, d=0.2, s=0.6, r=0.05)), **silent(3, 4, 5, 6), cutoff=8000, **seq([0, 4, 7, 12, 0, 4, 7, 12, 0, 5, 9, 12, 0, 5, 9, 12], gate=0.5, rate=1), props={'polyMode': MONO}, level=-8)
# Keys / Bass / Lead without the sequencer
TX('Keys', 'Toxin EP', algorithm=2, **ops(op(1, level=1, d=2.5, s=0.2, r=0.5, vel=0.7), op(4, ratio=14, level=0.2, d=0.25, s=0, vel=0.9), op(2, ratio=1, level=0.7, d=2.2, s=0.15, r=0.5), op(5, ratio=1, level=0.35, d=1.2, s=0, fb=0.2), op(3, ratio=2, level=0.3, d=1.5, s=0.1), op(6, ratio=3, level=0.2, d=0.4, s=0)), cutoff=8000, fxReverb=0.2, level=-6)
TX('Keys', 'FM Clav', algorithm=1, **ops(op(1, level=1, d=0.8, s=0.1, r=0.1, vel=0.9), op(2, ratio=3, level=0.5, d=0.15, s=0.15, vel=1), op(3, level=0.6, d=0.6, s=0.1, r=0.1), op(4, ratio=7, level=0.3, d=0.08, s=0.05), op(5, level=0.3, d=0.1, s=0.3), op(6, ratio=2, level=0.2, d=0.1, s=0.2)), cutoff=6000, fEnv=0.3, fD=0.15, fS=0, level=-6)
TX('Keys', 'Organ Toxin', algorithm=7, **ops(op(1, level=1, a=0.003, s=1, r=0.04), op(2, ratio=2, level=0.6, a=0.003, s=1, r=0.04), op(3, ratio=3, level=0.4, a=0.003, s=1, r=0.04), op(4, ratio=0.5, level=0.5, a=0.003, s=1, r=0.04)), **silent(5, 6), cutoff=7000, drive=0.2, level=-8)
TX('Bass', 'Toxin Sub', algorithm=0, **ops(op(1, level=1, d=1.5, s=0.9, r=0.15), op(2, ratio=1, level=0.25, d=0.3, s=0.1)), **silent(3, 4, 5, 6), cutoff=500, props={'polyMode': MONO, 'glideMode': 2}, glide=0.06, level=-3)
TX('Bass', 'FM Growler', True, algorithm=0, **ops(op(1, level=1, d=1, s=0.8, r=0.1), op(2, wave=SHARP, ratio=1, level=0.6, d=0.5, s=0.5, fb=0.5), op(3, ratio=0.5, level=0.4, d=0.3, s=0.4)), **silent(4, 5, 6), cutoff=1200, res=0.3, lfoRate=0.5, mods=[(LFO1, 'index', 0.5), (M1, 'cutoff', 0.7)], drive=0.45, driveType=1, props={'polyMode': MONO}, level=-7)
TX('Bass', 'Punch Bass', algorithm=0, **ops(op(1, level=1, d=0.6, s=0.5, r=0.08, vel=0.7), op(2, ratio=1, level=0.6, d=0.12, s=0.15, fb=0.35, vel=1), op(3, ratio=3, level=0.3, d=0.06, s=0)), **silent(4, 5, 6), cutoff=1500, fEnv=0.5, fD=0.1, fS=0, props={'polyMode': MONO}, level=-5)
TX('Bass', 'Hollow Bass', algorithm=0, **ops(op(1, level=1, d=0.8, s=0.6, r=0.1), op(2, ratio=2, level=0.45, d=0.3, s=0.3), op(3, ratio=4, level=0.15, d=0.1, s=0)), **silent(4, 5, 6), cutoff=900, res=0.4, fEnv=0.4, fD=0.2, fS=0.1, props={'polyMode': MONO}, level=-5)
TX('Lead', 'Toxin Lead', algorithm=0, **ops(op(1, level=1, a=0.005, s=0.9, r=0.2), op(2, ratio=1, level=0.55, s=0.6, fb=0.3), op(3, ratio=2, level=0.25, s=0.4)), **silent(4, 5, 6), cutoff=4000, res=0.2, lfoRate=5.5, mods=[(WHEEL, 'pitch', 0.03), (LFO1, 'pitch', 0.01)], drive=0.3, props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.06, fxDelay=0.25, fxDelayTime=0.75, level=-7)
TX('Lead', 'Screech', True, algorithm=0, **ops(op(1, wave=SHARP, level=1, s=1, r=0.15), op(2, ratio=1.5, level=0.6, s=0.7, fb=0.6), op(3, ratio=3, level=0.3, s=0.5)), **silent(4, 5, 6), cutoff=3000, res=0.5, fEnv=0.4, drive=0.6, driveType=2, props={'polyMode': MONO, 'glideMode': 2}, glide=0.04, level=-9)
TX('Lead', 'Soft Sine Lead', algorithm=0, **ops(op(1, level=1, a=0.02, s=1, r=0.3), op(2, ratio=2, level=0.15, s=0.4)), **silent(3, 4, 5, 6), cutoff=6000, lfoRate=5, mods=[(LFO1, 'pitch', 0.012)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.08, fxReverb=0.3, level=-6)
TX('Pad', 'Toxin Strings', algorithm=5, **ops(op(1, level=1, a=0.6, s=1, r=1.5), op(2, ratio=1, level=0.35, a=0.8, s=0.7), op(3, level=0.9, fine=7, a=0.7, s=1, r=1.5), op(4, ratio=2, level=0.25, a=0.9, s=0.6), op(5, level=0.9, fine=-7, a=0.8, s=1, r=1.5), op(6, ratio=3, level=0.2, a=1.0, s=0.5)), cutoff=5000, fxReverb=0.4, fxSize=0.8, level=-10)
TX('Pad', 'Dark Toxin Pad', True, algorithm=0, **ops(op(1, level=1, a=1.5, s=1, r=3), op(2, wave=SOFT, ratio=0.5, level=0.5, a=3, s=1, fb=0.4), op(3, ratio=0.25, level=0.4, a=4, s=1)), **silent(4, 5, 6), cutoff=1200, res=0.3, lfoRate=0.06, mods=[(LFO1, 'index', 0.6), (LFO1, 'cutoff', 0.4)], fxReverb=0.6, fxSize=0.9, level=-10)
TX('Pad', 'Sequenced Pad', True, algorithm=2, **ops(op(1, level=1, a=0.1, s=1, r=1), op(4, ratio=2, level=0.3, a=0.1, s=0.7), op(2, ratio=1, fine=6, level=0.7, a=0.1, s=1, r=1), op(5, ratio=3, level=0.2, s=0.6), op(3, ratio=0.5, level=0.5, a=0.1, s=1, r=1), op(6, level=0, s=0)), cutoff=2500, **seq([0, 0, 0, 0, 3, 3, 3, 3, 7, 7, 7, 7, 5, 5, 5, 5], rate=0, gate=0.95, slide=[1] * 16), fxReverb=0.5, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, props={'polyMode': MONO, 'glideMode': 2}, glide=0.2, level=-10)
TX('FX', 'Toxin Zap', True, algorithm=0, **ops(op(1, level=1, d=0.3, s=0, r=0.1), op(2, ratio=1, level=0.8, d=0.2, s=0, fb=0.6), op(3, ratio=4, level=0.5, d=0.1, s=0)), **silent(4, 5, 6), cutoff=12000, mods=[(ENV2, 'pitch', 1.0)], fA=0.001, fD=0.25, fS=0, props={'polyMode': MONO}, level=-6)
TX('FX', 'Metal Pulse', True, algorithm=3, **ops(op(1, level=1, d=0.6, s=0, r=0.3), op(2, ratio=1.41, level=0.7, d=0.4, s=0, fb=0.5), op(3, ratio=3.7, level=0.6, d=0.3, s=0), op(4, ratio=5.3, level=0.5, d=0.2, s=0), op(5, ratio=11, level=0.4, d=0.1, s=0), op(6, ratio=13, level=0.3, d=0.1, s=0)), cutoff=9000, **seq([0, 0, 12, 0, 0, 7, 0, 12] * 2, acc=[1, 0, 0, 0] * 4, gate=0.3), fxDelay=0.3, fxDelayTime=0.375, fxDelayFb=0.5, level=-8)
TX('FX', 'Toxin Drone', True, algorithm=6, **ops(op(1, level=1, a=2, s=1, r=3), op(2, ratio=1.5, level=0.6, a=2, s=1, r=3), op(3, ratio=2, level=0.5, a=2, s=1, r=3), op(4, ratio=3, level=0.3, a=2, s=1, r=3), op(5, ratio=0.5, level=0.6, a=2, s=1, r=3), op(6, wave=SOFT, ratio=0.25, level=0.4, a=3, s=1, fb=0.5)), cutoff=2000, res=0.4, lfoRate=0.08, mods=[(LFO1, 'op6', 0.6), (LFO1, 'cutoff', 0.3)], fxReverb=0.7, fxSize=0.95, level=-10)
TX('FX', 'Noise Hat Seq', True, algorithm=0, **ops(op(1, level=1, d=0.08, s=0, r=0.03), op(2, wave=SHARP, ratio=7.13, level=0.9, d=0.08, s=0, fb=0.9), op(3, ratio=11.7, level=0.8, d=0.06, s=0, fb=0.7)), **silent(4, 5, 6), fType=HP24, cutoff=6000, **seq([24] * 16, acc=[0, 0, 1, 0] * 4, gate=0.3, rate=1), level=-9)
TX('FX', 'Laser Stab', True, algorithm=0, **ops(op(1, level=1, d=0.5, s=0, r=0.2), op(2, ratio=2, level=0.9, d=0.4, s=0, fb=0.3), op(3, ratio=5, level=0.5, d=0.15, s=0)), **silent(4, 5, 6), cutoff=10000, mods=[(ENV2, 'pitch', 0.6), (VEL, 'index', 0.5)], fA=0.001, fD=0.4, fS=0, level=-7)
TX('Arp', 'Fifth Bounce', algorithm=0, **ops(op(1, level=1, d=0.3, s=0.3, r=0.1), op(2, ratio=1, level=0.45, d=0.15, s=0.2), op(3, ratio=2, level=0.2, d=0.1, s=0)), **silent(4, 5, 6), cutoff=2000, fEnv=0.5, fD=0.12, fS=0, **seq([0, 7, 0, 7, 12, 7, 0, 7, 3, 10, 3, 10, 15, 10, 3, 10], acc=[1, 0, 0, 0] * 4), fxDelay=0.3, fxDelayTime=0.375, fxDelayFb=0.4, level=-6)
TX('Seq Bass', 'Phonk Slide', True, algorithm=0, **ops(op(1, level=1, d=1.0, s=0.7, r=0.12), op(2, ratio=1, level=0.35, d=0.3, s=0.2, fb=0.3)), **silent(3, 4, 5, 6), cutoff=700, res=0.2, drive=0.4, driveType=1, **seq([0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, -2, -2, 0, 0], on=[1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0], acc=[1, 0, 0, 0, 0, 0, 1, 0] * 2, slide=[0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0], swing=25), props={'polyMode': MONO, 'glideMode': 2}, glide=0.15, level=-4)
TX('Keys', 'Bell Keys', algorithm=1, **ops(op(1, level=1, d=1.5, s=0, r=0.8), op(2, ratio=4, level=0.3, d=0.5, s=0), op(3, ratio=3, level=0.5, d=1.2, s=0), op(4, level=0.2, d=0.3, s=0), op(5, level=0, s=0), op(6, level=0, s=0)), cutoff=10000, fxReverb=0.3, level=-7)

# ------------------------------------------------------------------------------------------------ Lantern (vintage poly)
LN = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Lantern', cat, name, adv, props, mods, **k)
LN('Pad', 'Init Lantern', fEnv=0, aS=1, aR=0.1, ensemble=0)
LN('Pad', "'72 Warm Strings", era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=9, cutoff=1400, res=0.1, fEnv=0.2, aA=0.5, aS=1, aR=1.4, fA=0.6, fS=0.6, ensemble=0.6, ensRate=0.5, drift=0.5, age=0.4, fxReverb=0.3, level=-9)
LN('Pad', "'78 Classic Pad", era=1, o1Wave=W_SAW, o2Wave=W_PULSE, o2Pw=0.35, o2Fine=-7, cutoff=1800, res=0.2, fEnv=0.3, aA=0.3, aS=1, aR=1.0, fA=0.4, fD=1.5, fS=0.5, ensemble=0.45, lfoRate=0.4, lfoPw=0.3, fxReverb=0.25, level=-9)
LN('Pad', "'83 Tight Poly", era=2, o1Wave=W_SQR, o2Wave=W_SAW, o2Semi=-12, cutoff=2600, res=0.15, fEnv=0.4, aA=0.05, aD=0.8, aS=0.7, aR=0.5, fD=0.6, fS=0.3, ensemble=0.3, drift=0.15, age=0.1, level=-8)
LN('Pad', 'Brass Ensemble', era=1, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=12, o1Oct=0, cutoff=900, res=0.15, fEnv=0.8, aA=0.08, aS=0.9, aR=0.3, fA=0.1, fD=0.5, fS=0.4, ensemble=0.5, drift=0.4, level=-8)
LN('Pad', 'Glass Ensemble', era=2, o1Wave=W_TRI, o2Wave=W_PULSE, o2Pw=0.2, o2Semi=12, cutoff=3500, fEnv=0.2, aA=0.8, aS=1, aR=2.0, ensemble=0.7, ensRate=0.8, fxReverb=0.45, fxSize=0.8, level=-10)
LN('Pad', 'Dark Drift', True, era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Semi=-12, o2Fine=4, subLevel=0.3, cutoff=600, res=0.3, fEnv=0.4, aA=1.5, aS=1, aR=3, fA=3, fS=0.5, drift=0.9, age=0.8, ensemble=0.4, lfoRate=0.1, lfoCut=0.3, fxReverb=0.5, fxSize=0.9, level=-10)
LN('Pad', 'Choir Lantern', era=1, o1Wave=W_PULSE, o1Pw=0.18, o2Wave=W_PULSE, o2Pw=0.82, o2Fine=6, fType=1, cutoff=1500, res=0.25, fEnv=0.1, aA=0.7, aS=1, aR=1.8, ensemble=0.65, lfoRate=0.3, lfoPw=0.5, fxReverb=0.45, level=-10)
LN('Pad', 'Lush Sweep', True, era=1, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=10, cutoff=500, res=0.35, fEnv=0.7, aA=0.6, aS=1, aR=2.5, fA=2.5, fD=2.0, fS=0.3, ensemble=0.5, mods=[(LFO1, 'pan', 0.4)], lfoRate=0.15, fxReverb=0.4, fxDelay=0.25, fxDelayTime=1.5, fxDelayFb=0.45, level=-10)
# Keys
LN('Keys', 'Vintage EP', era=0, o1Wave=W_TRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, cutoff=2500, fEnv=0.5, aD=1.5, aS=0.3, aR=0.4, fD=0.5, fS=0.1, ensemble=0.3, age=0.5, level=-7)
LN('Keys', 'Poly Clav', era=2, o1Wave=W_PULSE, o1Pw=0.12, o2Wave=W_SAW, o2Level=0.3, cutoff=1600, res=0.3, fEnv=0.7, aD=0.6, aS=0.2, aR=0.08, fD=0.12, fS=0.1, ensemble=0, level=-7)
LN('Keys', 'Soft Organ', era=1, o1Wave=W_SINE, o2Wave=W_SINE, o2Semi=12, o2Level=0.5, subLevel=0.4, cutoff=8000, fEnv=0, aA=0.004, aS=1, aR=0.05, ensemble=0.5, ensRate=1.2, level=-7)
LN('Keys', 'Harpsi Lantern', era=2, o1Wave=W_PULSE, o1Pw=0.08, o2Wave=W_PULSE, o2Pw=0.1, o2Semi=12, o2Level=0.4, cutoff=4000, fEnv=0.4, aD=1.0, aS=0, aR=0.15, fD=0.3, fS=0, ensemble=0, level=-8)
LN('Keys', 'Wurly Drift', era=0, o1Wave=W_TRI, o2Wave=W_TRI, o2Fine=6, cutoff=2200, fEnv=0.4, aD=1.8, aS=0.2, aR=0.4, fD=0.6, fS=0, drift=0.6, age=0.6, ensemble=0.2, level=-7)
LN('Keys', 'Piano-ish', era=2, o1Wave=W_SAW, o2Wave=W_TRI, o2Semi=12, o2Level=0.4, cutoff=3000, fEnv=0.6, aD=2.5, aS=0.1, aR=0.4, fD=0.8, fS=0, ensemble=0.1, level=-7)
# Bass
LN('Bass', 'Fat Mono Bass', era=0, o1Wave=W_SAW, o2Wave=W_SQR, o2Semi=-12, subLevel=0.3, cutoff=500, res=0.25, fEnv=0.6, aS=0.8, aR=0.1, fD=0.35, fS=0.1, ensemble=0, props={'polyMode': MONO}, level=-4)
LN('Bass', 'Pulse Bass', era=1, o1Wave=W_PULSE, o1Pw=0.3, o2Wave=W_SAW, o2Level=0.4, cutoff=700, res=0.2, fEnv=0.5, aD=0.5, aS=0.5, aR=0.1, fD=0.25, fS=0, ensemble=0, props={'polyMode': MONO}, level=-5)
LN('Bass', 'Sub Lantern', era=2, o1Wave=W_SINE, o1Level=1, o2Level=0, subLevel=0.6, cutoff=400, fEnv=0.1, aS=1, aR=0.15, ensemble=0, props={'polyMode': MONO, 'glideMode': 2}, glide=0.06, level=-3)
LN('Bass', 'Squelch Lantern', era=1, o1Wave=W_SAW, o2Level=0, cutoff=300, res=0.75, fEnv=0.8, aS=0.6, aR=0.1, fD=0.2, fS=0, ensemble=0, props={'polyMode': MONO, 'glideMode': 2}, glide=0.08, level=-6)
LN('Bass', 'Unison Bass', era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=-9, cutoff=900, res=0.2, fEnv=0.4, aS=0.9, aR=0.15, fD=0.3, fS=0.2, uniDetune=14, ensemble=0, props={'polyMode': MONO, 'unison': 3}, level=-7)
LN('Bass', 'Poly Bass Chords', era=2, o1Wave=W_SQR, o2Wave=W_SAW, o2Semi=-12, o2Fine=5, cutoff=1200, fEnv=0.5, aD=0.8, aS=0.4, aR=0.2, fD=0.4, fS=0.1, ensemble=0.2, level=-7)
# Lead
LN('Lead', 'Vintage Lead', era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=8, cutoff=2500, res=0.2, fEnv=0.3, aS=1, aR=0.3, lfoRate=5.5, lfoDelay=0.4, mods=[(WHEEL, 'pitch', 0.03)], glide=0.06, props={'polyMode': LEGATO, 'glideMode': 2}, level=-7)
LN('Lead', 'Whistle Lead', era=1, o1Wave=W_SINE, o2Wave=W_TRI, o2Level=0.2, o2Semi=12, cutoff=5000, fEnv=0.2, aA=0.05, aS=1, aR=0.3, lfoRate=6, lfoDelay=0.5, mods=[(LFO1, 'pitch', 0.012)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.08, fxReverb=0.3, level=-7)
LN('Lead', 'Sync Shout', era=2, o1Wave=W_SAW, o2Wave=W_SAW, o2Sync=1, o2Semi=7, cutoff=3000, fEnv=0.4, aS=1, aR=0.2, mods=[(ENV2, 'pitch', 0.0), (WHEEL, 'pitch', 0.03)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.05, level=-7)
LN('Lead', 'Square Lantern', era=1, o1Wave=W_SQR, o2Wave=W_PULSE, o2Pw=0.3, o2Level=0.4, cutoff=2800, res=0.2, fEnv=0.3, aS=0.9, aR=0.25, props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.07, fxDelay=0.2, fxDelayTime=0.75, level=-6)
LN('Lead', 'Ensemble Lead', era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=14, cutoff=3500, fEnv=0.2, aS=1, aR=0.4, ensemble=0.7, props={'unison': 2}, uniDetune=12, fxReverb=0.25, level=-9)
# Pluck / Brass / FX
LN('Pluck', 'Vintage Pluck', era=1, o1Wave=W_SAW, o2Wave=W_SQR, o2Semi=12, o2Level=0.35, cutoff=500, res=0.25, fEnv=0.9, aD=0.4, aS=0, aR=0.2, fD=0.15, fS=0, ensemble=0.2, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6)
LN('Pluck', 'Marimba Drift', era=0, o1Wave=W_SINE, o2Wave=W_TRI, o2Semi=24, o2Level=0.3, cutoff=3000, fEnv=0.6, aD=0.5, aS=0, aR=0.3, fD=0.08, fS=0, drift=0.5, ensemble=0, level=-5)
LN('Pluck', 'Poly Pluck Wide', era=2, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=12, cutoff=700, res=0.2, fEnv=1, aD=0.5, aS=0, aR=0.35, fD=0.25, fS=0, ensemble=0.6, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.45, fxReverb=0.3, level=-7)
LN('Pluck', 'Kalimba Lantern', era=1, o1Wave=W_TRI, o2Wave=W_SINE, o2Semi=12, cutoff=2600, fEnv=0.5, aD=0.7, aS=0, aR=0.4, fD=0.1, fS=0, ensemble=0.2, fxReverb=0.25, level=-5)
LN('Brass', 'Poly Brass', era=1, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=7, cutoff=800, res=0.15, fEnv=0.8, aA=0.04, aD=0.4, aS=0.8, aR=0.15, fA=0.04, fD=0.3, fS=0.5, ensemble=0.3, props={'unison': 2}, uniDetune=8, level=-8)
LN('Brass', 'Horn Section', era=0, o1Wave=W_SAW, o2Wave=W_PULSE, o2Pw=0.4, o2Fine=-6, cutoff=1100, fEnv=0.6, aA=0.08, aS=0.9, aR=0.2, fA=0.12, fS=0.4, ensemble=0.5, drift=0.5, level=-8)
LN('Brass', 'Soft Horns', era=2, o1Wave=W_SAW, o2Wave=W_TRI, o2Fine=5, cutoff=1300, fEnv=0.4, aA=0.15, aS=1, aR=0.5, fA=0.3, fS=0.5, ensemble=0.4, lfoRate=4.5, lfoDelay=0.8, mods=[(LFO1, 'pitch', 0.006)], level=-8)
LN('FX', 'Drift Sweep', True, era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=20, cutoff=300, res=0.5, fEnv=1, aA=3, aS=1, aR=1, fA=4, fS=1, drift=1, ensemble=0.6, fxReverb=0.5, level=-10)
LN('FX', 'Self-Osc Whistle', True, era=1, o1Level=0, o2Level=0, cutoff=800, res=1, fEnv=0.5, fKey=1, aS=1, aR=0.3, fD=0.3, fS=0.6, props={'polyMode': MONO}, level=-10)
LN('FX', 'Noise Lantern', True, era=2, o1Level=0, o2Level=0, noiseLevel=1, fType=1, cutoff=1200, res=0.4, fEnv=0.6, aA=1.5, aS=1, aR=2.5, lfoRate=0.2, lfoCut=0.8, ensemble=0.5, fxReverb=0.5, fxSize=0.9, level=-9)
LN('FX', 'Siren Era', True, era=0, o1Wave=W_SAW, o2Wave=W_SQR, o2Semi=7, cutoff=2000, aS=1, aR=0.3, lfoShape=1, lfoRate=0.4, lfoPitch=0.5, props={'polyMode': MONO}, level=-8)
LN('Keys', 'Chorus Keys', era=1, o1Wave=W_PULSE, o1Pw=0.4, o2Wave=W_TRI, o2Fine=8, cutoff=2800, fEnv=0.4, aD=1.5, aS=0.5, aR=0.5, fD=0.6, fS=0.2, ensemble=0.75, ensRate=0.7, level=-7)
LN('Pad', 'Vintage Vox', era=0, o1Wave=W_PULSE, o1Pw=0.25, o2Wave=W_SAW, o2Fine=5, fType=1, cutoff=1100, res=0.4, aA=0.6, aS=1, aR=1.5, ensemble=0.6, lfoRate=0.25, lfoCut=0.2, fxReverb=0.4, level=-10)
LN('Lead', 'Portamento Solo', era=0, o1Wave=W_SAW, o2Wave=W_SAW, o2Semi=-12, cutoff=1800, res=0.3, fEnv=0.5, aS=1, aR=0.2, fD=0.5, fS=0.4, glide=0.12, lfoRate=5.5, lfoDelay=0.6, mods=[(LFO1, 'pitch', 0.01)], props={'polyMode': MONO, 'glideMode': 1}, level=-6)

# ------------------------------------------------------------------------------------------------ Lantern Exo (wavetables + analogue)
EX = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Lantern Exo', cat, name, adv, props, mods, **k)
T_SINE, T_FOLD, T_SAWTRI, T_VOCAL, T_ORGAN, T_BRIGHT, T_GLASS, T_DIGITAL = [i / 7.0 for i in range(8)]
EX('Pad', 'Init Exo', f1Env=0, aS=1, aR=0.1)
EX('Pad', 'Morphing Pad', o1Wave=W_TABLE, o1Morph=0.35, o2Wave=W_TABLE, o2Morph=0.6, o2Fine=7, f1Cut=2500, f1Env=0.2, aA=0.8, aS=1, aR=2.0, fA=1.5, fS=0.6, lfo1Rate=0.08, mods=[(LFO1, 'morph', 0.5), (LFO2, 'pan', 0.4)], lfo2Rate=0.13, fxChorus=0.4, fxReverb=0.45, fxSize=0.85, props={'unison': 2}, uniDetune=10, width=1.5, level=-10)
EX('Pad', 'Vocal Pad', o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SAW, o2Level=0.4, o2Fine=-8, f1Cut=2000, f1Res=0.2, aA=0.6, aS=1, aR=1.8, mods=[(LFO1, 'morph', 0.15)], lfo1Rate=0.3, fxChorus=0.5, fxReverb=0.4, level=-10)
EX('Pad', 'Glass Exo', o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_GLASS, o2Semi=12, o2Level=0.4, f1Cut=4000, aA=1.2, aS=1, aR=2.5, lfo1Rate=0.1, mods=[(LFO1, 'morph', 0.1)], fxReverb=0.55, fxSize=0.9, fxDelay=0.3, fxDelayTime=1.5, fxDelayFb=0.5, level=-11)
EX('Pad', 'Digital Strings', o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=10, o1Fine=-10, f1Cut=1800, f1Env=0.25, aA=0.5, aS=1, aR=1.6, drift=0.4, props={'unison': 4}, uniDetune=14, fxChorus=0.5, fxReverb=0.35, level=-11)
EX('Pad', 'Fold Drone', True, o1Wave=W_TABLE, o1Morph=T_FOLD, o2Wave=W_TABLE, o2Morph=T_FOLD, o2Semi=-12, o2Fine=3, f1Type=LP24, f1Cut=900, f1Res=0.3, aA=2.5, aS=1, aR=4, lfo1Rate=0.05, lfo2Rate=0.07, mods=[(LFO1, 'morph', 0.7), (LFO2, 'cutoff', 0.4)], fxReverb=0.7, fxSize=0.95, level=-10)
EX('Pad', 'Organ Pad', o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_TABLE, o2Morph=T_SINE, o2Semi=12, o2Level=0.4, f1Cut=5000, aA=0.4, aS=1, aR=1.0, fxChorus=0.45, fxReverb=0.3, level=-9)
EX('Pad', 'Dual Filter Sweep', True, o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_SQR, o2Semi=-12, f1Type=LP24, f1Cut=600, f1Res=0.35, routing=1, f2Type=HP12, f2Cut=200, f2Env=0.5, f1Env=0.6, fA=3, fD=2, fS=0.3, aA=1.0, aS=1, aR=3, lfo1Rate=0.1, mods=[(LFO1, 'morph', 0.4)], fxReverb=0.5, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.5, level=-11)
EX('Pad', 'Warm Exo', o1Wave=W_SAW, o2Wave=W_TABLE, o2Morph=T_SAWTRI, o2Fine=12, subLevel=0.2, f1Type=LADDER, f1Cut=1400, f1Res=0.12, aA=0.4, aS=0.9, aR=1.2, drift=0.5, drive=0.1, driveType=1, fxChorus=0.3, props={'unison': 2}, level=-9)
# Keys
EX('Keys', 'Wavetable EP', o1Wave=W_TABLE, o1Morph=T_SINE + 0.04, o2Wave=W_SINE, o2Semi=24, o2Level=0.2, fmAmt=0.2, f1Cut=5000, aD=1.8, aS=0.3, aR=0.4, ampVel=1, mods=[(VEL, 'fm', 0.5), (ENV2, 'morph', 0.3)], fD=0.4, fS=0, fxChorus=0.15, fxReverb=0.15, level=-6)
EX('Keys', 'Glass Keys', o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=6000, f1Env=0.3, aD=2.0, aS=0.2, aR=0.6, fD=0.5, fS=0, ampVel=0.9, fxReverb=0.3, level=-7)
EX('Keys', 'Digital Organ', o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_TABLE, o2Morph=T_ORGAN, o2Semi=12, o2Level=0.5, f1Cut=20000, f1Env=0, aA=0.003, aS=1, aR=0.04, fxChorus=0.35, level=-7)
EX('Keys', 'Vocal Keys', o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_TRI, o2Level=0.3, f1Cut=3000, f1Env=0.4, aD=1.2, aS=0.4, aR=0.4, fD=0.5, fS=0.2, mods=[(ENV2, 'morph', 0.2)], fxReverb=0.2, level=-7)
EX('Keys', 'Clav Exo', o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_PULSE, o2Pw=0.12, o2Level=0.3, f1Cut=1800, f1Res=0.3, f1Env=0.6, fD=0.12, fS=0.1, aD=0.6, aS=0.2, aR=0.08, ampVel=0.9, level=-7)
EX('Keys', 'Bell Exo', True, o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=19, o2Fine=4, o2Level=0.45, fmAmt=0.12, f1Cut=8000, aD=2.5, aS=0, aR=1.5, fxReverb=0.35, fxSize=0.7, mods=[(KEY, 'amp', -0.3)], level=-7)
# Bass
EX('Bass', 'Digital Sub', o1Wave=W_TABLE, o1Morph=T_SINE, o1Level=1, o2Level=0, subLevel=0.5, f1Cut=400, f1Env=0.1, aD=0.5, aS=0.9, aR=0.15, props={'polyMode': MONO, 'glideMode': 2}, glide=0.06, level=-3)
EX('Bass', 'Fold Bass', o1Wave=W_TABLE, o1Morph=T_FOLD, o2Wave=W_SAW, o2Semi=-12, o2Level=0.5, f1Cut=800, f1Res=0.25, f1Env=0.5, fD=0.3, fS=0.1, aS=0.9, aR=0.15, drive=0.3, driveType=1, mods=[(ENV2, 'morph', 0.4)], props={'polyMode': MONO}, level=-5)
EX('Bass', 'Reese Exo', o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=18, o1Fine=-18, subLevel=0.35, f1Cut=900, f1Res=0.25, uniDetune=20, uniSpread=1, aS=1, aR=0.2, props={'unison': 4, 'polyMode': MONO}, width=1.4, level=-8)
EX('Bass', 'Growl Table', True, o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SQR, o2Semi=-12, f1Type=BP, f1Cut=700, f1Res=0.5, routing=1, f2Type=HP12, f2Cut=60, lfo1Shape=0, lfo1Rate=0.8, lfo1Sync=1, mods=[(LFO1, 'morph', 0.7), (M1, 'cutoff', 0.8)], drive=0.5, driveType=3, aS=1, aR=0.15, props={'polyMode': MONO}, level=-8)
EX('Bass', 'Pluck Bass Exo', o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_TRI, o2Semi=-12, subLevel=0.4, f1Cut=320, f1Res=0.3, f1Env=0.85, fD=0.28, fS=0, aD=0.45, aS=0.2, aR=0.12, ampVel=0.8, level=-5)
EX('Bass', 'Acid Exo', o1Wave=W_SAW, o2Level=0, f1Type=LADDER, f1Cut=380, f1Res=0.72, f1Env=0.75, fD=0.22, fS=0, aD=0.3, aS=0.6, aR=0.1, drive=0.35, driveType=1, glide=0.09, props={'polyMode': MONO, 'glideMode': 2}, mods=[(MODX, 'cutoff', 0.6)], level=-6)
EX('Bass', 'Wobble Table', True, o1Wave=W_TABLE, o1Morph=T_DIGITAL, o2Wave=W_SQR, o2Semi=-12, subLevel=0.3, f1Cut=300, f1Res=0.45, lfo1Shape=1, lfo1Sync=1, lfo1Rate=6, lfo1Cut=0.9, mods=[(LFO1, 'morph', 0.5)], drive=0.5, driveType=2, aS=1, aR=0.1, props={'polyMode': MONO}, level=-8)
# Lead
EX('Lead', 'Bright Table Lead', o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_SAW, o2Fine=10, f1Cut=5000, f1Res=0.1, aA=0.01, aS=0.9, aR=0.35, uniDetune=16, uniSpread=1, props={'unison': 4}, mods=[(WHEEL, 'morph', 0.3)], fxChorus=0.2, fxReverb=0.18, width=1.5, level=-9)
EX('Lead', 'Vocal Lead', o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Level=0, f1Cut=4000, f1Env=0.2, aS=1, aR=0.25, glide=0.06, lfo1Rate=5.5, mods=[(WHEEL, 'morph', 0.5), (LFO1, 'pitch', 0.01)], props={'polyMode': LEGATO, 'glideMode': 2}, fxDelay=0.2, fxDelayTime=0.75, level=-7)
EX('Lead', 'Digital Scream', True, o1Wave=W_TABLE, o1Morph=T_DIGITAL, o2Wave=W_SAW, o2Sync=1, o2Semi=7, f1Cut=3000, f1Env=0.4, aS=1, aR=0.2, drive=0.4, driveType=1, glide=0.05, props={'polyMode': LEGATO, 'glideMode': 2}, mods=[(ENV3, 'morph', 0.6)], mA=0.001, mD=0.5, mS=0, level=-8)
EX('Lead', 'Sine Table Solo', o1Wave=W_TABLE, o1Morph=T_SINE + 0.02, o2Level=0, f1Cut=6000, aS=1, aR=0.3, lfo1Rate=5.5, mods=[(WHEEL, 'pitch', 0.03), (LFO1, 'pitch', 0.012), (AT, 'morph', 0.4)], props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.07, fxReverb=0.25, level=-6)
EX('Lead', 'Glass Lead', o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_GLASS, o2Fine=8, f1Cut=5000, aA=0.02, aS=0.9, aR=0.4, props={'unison': 2}, uniDetune=10, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.4, fxReverb=0.3, level=-8)
# Pluck
EX('Pluck', 'Table Pluck', o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_SQR, o2Semi=12, o2Level=0.35, f1Cut=500, f1Res=0.25, f1Env=0.9, fD=0.16, fS=0, aD=0.35, aS=0, aR=0.2, mods=[(ENV2, 'morph', -0.4)], fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6)
EX('Pluck', 'Glass Pluck', o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, f1Cut=3000, f1Env=0.5, fD=0.1, fS=0, aD=0.7, aS=0, aR=0.4, fxReverb=0.3, level=-5)
EX('Pluck', 'Future Pluck Exo', True, o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=15, o1Fine=-15, uniDetune=20, f1Cut=700, f1Res=0.2, f1Env=1, fD=0.25, fS=0, aD=0.5, aS=0, aR=0.35, mods=[(ENV2, 'morph', 0.5)], fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.45, fxReverb=0.3, props={'unison': 4}, width=1.5, level=-8)
EX('Pluck', 'Vocal Pluck', o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Level=0, f1Cut=2500, f1Env=0.6, fD=0.15, fS=0, aD=0.5, aS=0, aR=0.3, mods=[(ENV2, 'morph', 0.3)], fxDelay=0.2, fxDelayTime=0.375, level=-6)
EX('Pluck', 'Organ Stab', o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_TABLE, o2Morph=T_ORGAN, o2Semi=12, o2Level=0.4, f1Cut=4000, f1Env=0.3, aD=0.3, aS=0, aR=0.2, fD=0.1, fS=0, fxReverb=0.25, level=-6)
# FX / Seq
EX('FX', 'Morph Riser', True, o1Wave=W_TABLE, o1Morph=0, o2Wave=W_NOISE, o2Level=0.4, f1Type=BP, f1Cut=300, f1Res=0.5, f1Env=1, fA=4.0, fS=1, aA=3.5, aS=1, aR=0.5, mods=[(ENV2, 'morph', 1.0), (ENV2, 'pitch', 0.4)], props={'unison': 4}, uniDetune=30, fxReverb=0.5, level=-10)
EX('FX', 'Digital Rain', True, o1Wave=W_TABLE, o1Morph=T_DIGITAL, o2Level=0, f1Type=BP, f1Cut=2000, f1Res=0.5, aD=0.15, aS=0, aR=0.1, lfo1Shape=4, lfo1Rate=8, mods=[(LFO1, 'morph', 0.8), (LFO1, 'pitch', 0.3)], fxDelay=0.4, fxDelayTime=0.375, fxDelayFb=0.6, fxReverb=0.4, level=-8)
EX('FX', 'Alien Talk Exo', True, o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SINE, o2Semi=5, ring=0.6, f1Type=BP, f1Cut=900, f1Res=0.6, lfo1Shape=4, lfo1Rate=7, lfo1Cut=0.8, mods=[(LFO1, 'morph', 0.9)], aS=1, aR=0.2, level=-8)
EX('FX', 'Sub Drop Exo', True, o1Wave=W_SINE, o2Level=0, subLevel=0.5, f1Cut=20000, aA=0.01, aD=3, aS=0, aR=0.5, mods=[(ENV2, 'pitch', 1.0)], fA=0.001, fD=2.5, fS=0, props={'polyMode': MONO}, level=-2)
EX('FX', 'Shimmer Wash', True, o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_SINE, o2Semi=19, o2Level=0.4, f1Cut=6000, aA=2.0, aS=1, aR=4.0, lfo1Rate=0.06, mods=[(LFO1, 'morph', 0.5)], fxReverb=0.75, fxSize=0.95, fxDelay=0.4, fxDelayTime=1.5, fxDelayFb=0.6, level=-11)
EX('Pad', 'Exo Choir', o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_TABLE, o2Morph=T_VOCAL + 0.05, o2Fine=7, f1Type=BP, f1Cut=1200, f1Res=0.3, aA=0.9, aS=1, aR=2.0, lfo2Rate=0.25, mods=[(LFO2, 'morph', 0.2)], fxChorus=0.45, fxReverb=0.5, fxSize=0.85, level=-10)
EX('Keys', 'Soft Exo Keys', o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=3200, f1Env=0.4, fD=0.6, fS=0.2, aD=1.2, aS=0.5, aR=0.5, ampVel=0.9, fxReverb=0.2, level=-7)
EX('Bass', 'Table Stab Bass', o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_SAW, o2Semi=-12, o2Level=0.6, f1Type=LADDER, f1Cut=250, f1Res=0.4, f1Env=0.7, fD=0.35, fS=0.05, aD=0.5, aS=0.5, aR=0.1, drive=0.2, props={'polyMode': MONO}, level=-5)

# ------------------------------------------------------------------------------------------------ Larva Bass (acid)
LB = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Larva Bass', cat, name, adv, props, mods, **k)
LB('Acid', 'Init Larva', cutoff=520, res=0.65, envMod=0.55, decay=0.3, accentAmt=0.6)
LB('Acid', 'Classic Acid', wave=0, cutoff=420, res=0.7, envMod=0.65, decay=0.28, accentAmt=0.7, level=-6)
LB('Acid', 'Square Acid', wave=1, cutoff=380, res=0.75, envMod=0.6, decay=0.32, accentAmt=0.65, level=-6)
LB('Acid', 'Deep Squelch', wave=0, cutoff=260, res=0.85, envMod=0.8, decay=0.4, accentAmt=0.8, drive=0.15, level=-7)
LB('Acid', 'Bright Acid', wave=0, cutoff=900, res=0.6, envMod=0.5, decay=0.2, accentAmt=0.6, tone=0.7, level=-6)
LB('Acid', 'Rubber Acid', wave=1, cutoff=300, res=0.55, envMod=0.9, decay=0.15, accentAmt=0.5, sub=0.2, level=-5)
LB('Acid', 'Slow Acid', wave=0, cutoff=350, res=0.7, envMod=0.7, decay=0.8, accentAmt=0.6, slideTime=0.15, level=-6)
LB('Acid', 'Distorted Acid', True, wave=0, cutoff=500, res=0.7, envMod=0.6, decay=0.3, accentAmt=0.7, drive=0.6, driveType=2, tone=0.5, level=-9)
LB('Acid', 'Tube Acid', True, wave=0, cutoff=450, res=0.65, envMod=0.6, decay=0.3, accentAmt=0.7, drive=0.4, driveType=1, level=-7)
LB('Acid', 'Fold Acid', True, wave=1, cutoff=600, res=0.6, envMod=0.5, decay=0.25, accentAmt=0.7, drive=0.5, driveType=3, level=-8)
LB('Acid', 'Dub Acid', True, wave=0, cutoff=320, res=0.7, envMod=0.6, decay=0.35, accentAmt=0.6, fxDelay=0.35, fxDelayTime=0.375, fxDelayFb=0.55, level=-7)
LB('Acid', 'Hollow Acid', wave=1, cutoff=700, res=0.8, envMod=0.4, decay=0.3, accentAmt=0.6, sub=0, tone=0.4, level=-6)
# Sequences
LB('Sequence', 'Acid Line 1', wave=0, cutoff=420, res=0.7, envMod=0.65, decay=0.28, accentAmt=0.7, **ACID1, level=-6)
LB('Sequence', 'Acid Line 2', wave=1, cutoff=380, res=0.75, envMod=0.6, decay=0.3, accentAmt=0.7, **ACID2, level=-6)
LB('Sequence', 'Octave Drive', wave=0, cutoff=500, res=0.6, envMod=0.6, decay=0.22, accentAmt=0.6, **OCT, drive=0.25, level=-6)
LB('Sequence', 'Minor Walk', wave=0, cutoff=450, res=0.65, envMod=0.6, decay=0.3, accentAmt=0.6, **MINOR, level=-6)
LB('Sequence', 'Trance Drive', wave=0, cutoff=600, res=0.6, envMod=0.6, decay=0.2, accentAmt=0.6, **TRANCE, level=-6)
LB('Sequence', 'Triplet Squelch', True, wave=1, cutoff=350, res=0.75, envMod=0.7, decay=0.25, accentAmt=0.7, **seq([0, 0, 12, 0, 3, 12, 0, 7, 12, 0, 0, 12], acc=[1, 0, 0] * 5 + [1], rate=3), level=-6)
LB('Sequence', 'Slide Walk', True, wave=0, cutoff=400, res=0.7, envMod=0.6, decay=0.35, accentAmt=0.6, slideTime=0.1, **seq([0, 2, 3, 5, 7, 5, 3, 2, 0, -2, -5, -2, 0, 3, 7, 12], slide=[1] * 16, acc=[1, 0, 0, 0] * 4, gate=0.9), level=-6)
LB('Sequence', 'Rest Groove', wave=0, cutoff=480, res=0.65, envMod=0.6, decay=0.25, accentAmt=0.7, **seq([0, 0, 12, 0, 0, 0, 7, 0, 0, 12, 0, 0, 3, 0, 0, 10], on=[1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1], acc=[1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1], swing=15), level=-6)
LB('Sequence', 'Swing Acid', wave=1, cutoff=420, res=0.7, envMod=0.6, decay=0.3, accentAmt=0.7, **seq([0, 0, 0, 12, 0, 0, 10, 0, 0, 0, 0, 12, 0, 7, 0, 3], acc=[1, 0, 0, 1, 0, 0, 1, 0] * 2, swing=35), level=-6)
LB('Sequence', '1/32 Rush', True, wave=0, cutoff=700, res=0.6, envMod=0.5, decay=0.15, accentAmt=0.6, **seq([0, 0, 12, 0, 0, 12, 0, 12] * 2, acc=[1, 0, 0, 0, 1, 0, 0, 0] * 2, rate=2, gate=0.5), level=-6)
LB('Sequence', 'Octave Down Pump', wave=0, cutoff=400, res=0.6, envMod=0.6, decay=0.25, accentAmt=0.6, sub=0.3, **seq([0, 12] * 8, acc=[1, 0, 0, 0] * 4, octave=-1), level=-5)
LB('Sequence', 'Up Two Octaves', True, wave=1, cutoff=900, res=0.65, envMod=0.5, decay=0.2, accentAmt=0.6, **seq([0, 3, 7, 12, 0, 3, 7, 10, 0, 3, 7, 12, 15, 12, 7, 3], acc=[1, 0, 0, 0] * 4, octave=1), fxDelay=0.3, fxDelayTime=0.375, fxDelayFb=0.4, level=-7)
LB('Sequence', 'Drill Pattern', True, wave=0, cutoff=350, res=0.5, envMod=0.4, decay=0.5, accentAmt=0.5, sub=0.4, **DRILL, drive=0.3, driveType=1, level=-4)
LB('Sequence', 'Techno Pulse', wave=0, cutoff=550, res=0.7, envMod=0.7, decay=0.18, accentAmt=0.8, **seq([0] * 16, on=[1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1], acc=[1, 0, 0, 0] * 4), level=-6)
LB('Sequence', 'Fifth Hop', wave=1, cutoff=480, res=0.65, envMod=0.6, decay=0.25, accentAmt=0.6, **seq([0, 7, 0, 7, 0, 7, 12, 7, 0, 7, 0, 7, 3, 7, 10, 7], acc=[1, 0, 0, 0] * 4), level=-6)
# Bass (no sequencer)
LB('Bass', 'Sub Larva', wave=1, cutoff=200, res=0.2, envMod=0.2, decay=0.5, accentAmt=0.3, sub=0.6, level=-3)
LB('Bass', 'Punch Bass', wave=0, cutoff=600, res=0.3, envMod=0.5, decay=0.15, accentAmt=0.5, sub=0.3, level=-5)
LB('Bass', 'Driven Bass', wave=0, cutoff=800, res=0.3, envMod=0.4, decay=0.3, accentAmt=0.5, drive=0.5, driveType=1, tone=0.6, level=-8)
LB('Bass', 'Soft Square Bass', wave=1, cutoff=350, res=0.25, envMod=0.3, decay=0.4, accentAmt=0.4, tone=0.3, level=-5)
LB('Bass', 'Slide Bass', wave=0, cutoff=450, res=0.4, envMod=0.5, decay=0.4, accentAmt=0.5, slideTime=0.2, sub=0.3, level=-5)
LB('Bass', 'Hard Fold Bass', True, wave=1, cutoff=500, res=0.4, envMod=0.5, decay=0.25, accentAmt=0.6, drive=0.7, driveType=3, tone=0.5, level=-10)
LB('Bass', 'Dub Bass', wave=1, cutoff=250, res=0.3, envMod=0.4, decay=0.6, accentAmt=0.4, sub=0.5, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.5, level=-4)
LB('Bass', 'Clicky Bass', wave=0, cutoff=1500, res=0.5, envMod=0.8, decay=0.08, accentAmt=0.7, level=-6)
# Lead / FX
LB('Lead', 'Acid Lead', wave=0, cutoff=1500, res=0.6, envMod=0.5, decay=0.3, accentAmt=0.6, slideTime=0.08, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.4, level=-7)
LB('Lead', 'Screaming Larva', True, wave=0, cutoff=2500, res=0.85, envMod=0.6, decay=0.4, accentAmt=0.8, drive=0.6, driveType=2, fxDelay=0.3, fxDelayTime=0.375, fxDelayFb=0.5, level=-11)
LB('Lead', 'Hollow Lead', wave=1, cutoff=1800, res=0.7, envMod=0.4, decay=0.5, accentAmt=0.5, sub=0, tone=0.5, level=-7)
LB('FX', 'Res Zap', True, wave=0, cutoff=3000, res=0.95, envMod=1.0, decay=0.08, accentAmt=1.0, level=-9)
LB('FX', 'Larva Siren', True, wave=1, cutoff=1200, res=0.9, envMod=0.3, decay=2.0, accentAmt=0.5, slideTime=0.4, **seq([0, 12, 0, 12, 0, 12, 0, 12] * 2, slide=[1] * 16, rate=0, gate=0.98), level=-9)

# ------------------------------------------------------------------------------------------------ Thorn Bass (electric bass model)
TB = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Thorn Bass', cat, name, adv, props, mods, **k)
TB('Finger', 'Init Thorn', pickPos=0.2, pickupPos=0.3, pickupBlend=0.5)
TB('Finger', 'Studio Finger', pickPos=0.22, pickupPos=0.3, pickupBlend=0.45, tone=0.55, decay=0.7, body=0.35, ampDrive=0.25, ampTone=3200, fretNoiseAmt=0.3, roundRobin=0.5, level=-5)
TB('Finger', 'Motown Flat', pickPos=0.28, pickupPos=0.42, pickupBlend=0.15, tone=0.3, decay=0.45, body=0.5, ampDrive=0.3, ampTone=1800, mute=0.25, fretNoiseAmt=0.2, level=-4)
TB('Finger', 'Jazz Neck', pickPos=0.25, pickupPos=0.4, pickupBlend=0.2, tone=0.5, decay=0.8, body=0.45, ampDrive=0.15, ampTone=2800, fretNoiseAmt=0.25, level=-5)
TB('Finger', 'Bridge Bite', pickPos=0.15, pickupPos=0.12, pickupBlend=0.9, tone=0.7, decay=0.7, body=0.25, ampDrive=0.3, ampTone=4500, fretNoiseAmt=0.35, level=-5)
TB('Finger', 'Precision Punch', pickPos=0.2, pickupPos=0.33, pickupBlend=0.4, tone=0.6, decay=0.6, body=0.4, ampDrive=0.35, ampTone=3500, fretNoiseAmt=0.3, level=-5)
TB('Finger', 'Fretless Glide', True, pickPos=0.3, pickupPos=0.35, pickupBlend=0.35, tone=0.45, decay=0.9, body=0.55, ampDrive=0.15, ampTone=2600, fretNoiseAmt=0.05, glide=0.18, props={'polyMode': LEGATO, 'glideMode': 2}, fxChorus=0.25, level=-5)
TB('Finger', 'Upright-ish', True, pickPos=0.35, pickupPos=0.45, pickupBlend=0.05, tone=0.2, decay=0.4, body=0.8, ampDrive=0.1, ampTone=1500, mute=0.35, fretNoiseAmt=0.15, level=-4)
TB('Finger', 'Round Wound Bright', pickPos=0.18, pickupPos=0.3, pickupBlend=0.5, tone=0.85, decay=0.8, body=0.3, ampDrive=0.2, ampTone=5000, fretNoiseAmt=0.4, level=-6)
TB('Finger', 'Flat Wound Dark', pickPos=0.25, pickupPos=0.38, pickupBlend=0.3, tone=0.2, decay=0.5, body=0.45, ampDrive=0.2, ampTone=1600, fretNoiseAmt=0.1, level=-4)
# Pick
TB('Pick', 'Pick Rock', pickPos=0.12, pickupPos=0.25, pickupBlend=0.6, tone=0.75, decay=0.75, body=0.3, ampDrive=0.5, ampTone=4000, fretNoiseAmt=0.4, roundRobin=0.6, level=-7)
TB('Pick', 'Punk Pick', pickPos=0.1, pickupPos=0.2, pickupBlend=0.75, tone=0.9, decay=0.7, body=0.25, ampDrive=0.7, ampTone=4800, fretNoiseAmt=0.5, level=-9)
TB('Pick', 'Palm Mute Pick', pickPos=0.1, pickupPos=0.22, pickupBlend=0.7, tone=0.7, decay=0.6, body=0.3, ampDrive=0.45, ampTone=3500, mute=0.65, fretNoiseAmt=0.3, level=-6)
TB('Pick', 'Indie Pick', pickPos=0.15, pickupPos=0.3, pickupBlend=0.5, tone=0.65, decay=0.8, body=0.35, ampDrive=0.3, ampTone=3800, fretNoiseAmt=0.35, fxChorus=0.2, level=-6)
TB('Pick', 'Metal Pick', True, pickPos=0.08, pickupPos=0.15, pickupBlend=0.9, tone=0.95, decay=0.65, body=0.2, ampDrive=0.85, ampTone=5500, mute=0.3, fretNoiseAmt=0.5, level=-11)
TB('Pick', 'Pop Pick Clean', pickPos=0.16, pickupPos=0.3, pickupBlend=0.5, tone=0.7, decay=0.75, body=0.35, ampDrive=0.15, ampTone=4200, fretNoiseAmt=0.3, level=-5)
TB('Pick', 'Chorus Pick 80s', pickPos=0.14, pickupPos=0.28, pickupBlend=0.55, tone=0.8, decay=0.85, body=0.3, ampDrive=0.2, ampTone=5000, fretNoiseAmt=0.3, fxChorus=0.6, level=-6)
# Slap
TB('Slap', 'Slap Funk', pickPos=0.1, pickupPos=0.3, pickupBlend=0.5, tone=0.85, decay=0.7, body=0.3, ampDrive=0.3, ampTone=5000, slap=0.8, fretNoiseAmt=0.4, level=-7)
TB('Slap', 'Slap and Pop', pickPos=0.08, pickupPos=0.25, pickupBlend=0.6, tone=0.95, decay=0.6, body=0.25, ampDrive=0.35, ampTone=6000, slap=1.0, fretNoiseAmt=0.5, level=-8)
TB('Slap', 'Thumb Mute', pickPos=0.12, pickupPos=0.35, pickupBlend=0.3, tone=0.6, decay=0.4, body=0.4, ampDrive=0.3, ampTone=3500, slap=0.6, mute=0.45, fretNoiseAmt=0.3, level=-5)
TB('Slap', 'Gospel Slap', pickPos=0.1, pickupPos=0.3, pickupBlend=0.45, tone=0.8, decay=0.75, body=0.35, ampDrive=0.25, ampTone=4500, slap=0.7, fretNoiseAmt=0.35, fxChorus=0.2, level=-7)
TB('Slap', 'Disco Slap', pickPos=0.1, pickupPos=0.3, pickupBlend=0.55, tone=0.9, decay=0.65, body=0.3, ampDrive=0.3, ampTone=5500, slap=0.9, fretNoiseAmt=0.45, level=-8)
# Muted / Dub / Reggae
TB('Muted', 'Reggae Dub', pickPos=0.3, pickupPos=0.42, pickupBlend=0.1, tone=0.15, decay=0.35, body=0.6, ampDrive=0.2, ampTone=1200, mute=0.5, fretNoiseAmt=0.1, level=-3)
TB('Muted', 'Palm Mute Groove', pickPos=0.15, pickupPos=0.3, pickupBlend=0.5, tone=0.5, decay=0.5, body=0.4, ampDrive=0.35, ampTone=2500, mute=0.7, fretNoiseAmt=0.25, level=-4)
TB('Muted', 'Hip Hop Thump', pickPos=0.25, pickupPos=0.4, pickupBlend=0.2, tone=0.3, decay=0.45, body=0.5, ampDrive=0.4, ampTone=1800, mute=0.4, fretNoiseAmt=0.15, level=-3)
TB('Muted', 'Lo-Fi Mute', True, pickPos=0.28, pickupPos=0.4, pickupBlend=0.2, tone=0.1, decay=0.3, body=0.5, ampDrive=0.55, ampTone=1000, mute=0.6, fretNoiseAmt=0.2, level=-4)
TB('Muted', 'Staccato Pluck', pickPos=0.2, pickupPos=0.3, pickupBlend=0.5, tone=0.6, decay=0.3, body=0.35, ampDrive=0.25, ampTone=3000, mute=0.55, fretNoiseAmt=0.3, level=-4)
# Synthetic / Driven / FX
TB('Driven', 'Fuzz Bass', True, pickPos=0.12, pickupPos=0.2, pickupBlend=0.8, tone=0.8, decay=0.8, body=0.2, ampDrive=1.0, ampTone=3500, fretNoiseAmt=0.4, level=-12)
TB('Driven', 'Tube Warm', pickPos=0.2, pickupPos=0.32, pickupBlend=0.4, tone=0.5, decay=0.7, body=0.4, ampDrive=0.6, ampTone=2800, fretNoiseAmt=0.3, level=-8)
TB('Driven', 'Overdriven Pick', pickPos=0.1, pickupPos=0.25, pickupBlend=0.7, tone=0.85, decay=0.7, body=0.25, ampDrive=0.75, ampTone=4500, fretNoiseAmt=0.45, level=-10)
TB('Driven', 'Grit Finger', pickPos=0.2, pickupPos=0.3, pickupBlend=0.5, tone=0.6, decay=0.65, body=0.35, ampDrive=0.5, ampTone=3200, fretNoiseAmt=0.35, level=-7)
TB('Chords', 'Bass Chords', pickPos=0.18, pickupPos=0.35, pickupBlend=0.3, tone=0.55, decay=0.9, body=0.4, ampDrive=0.15, ampTone=3000, fretNoiseAmt=0.2, props={'polyMode': POLY, 'maxPoly': 4}, fxChorus=0.3, level=-8)
TB('Chords', 'Strummed Bass', pickPos=0.12, pickupPos=0.3, pickupBlend=0.5, tone=0.7, decay=0.85, body=0.35, ampDrive=0.25, ampTone=4000, fretNoiseAmt=0.4, props={'polyMode': POLY, 'maxPoly': 4}, level=-8)
TB('Chords', 'Tapping', True, pickPos=0.45, pickupPos=0.3, pickupBlend=0.5, tone=0.75, decay=0.75, body=0.3, ampDrive=0.3, ampTone=4200, fretNoiseAmt=0.6, props={'polyMode': POLY, 'maxPoly': 6}, fxChorus=0.2, level=-7)
TB('Synthetic', 'Sub Model', pickPos=0.5, pickupPos=0.5, pickupBlend=0.0, tone=0.05, decay=1.0, body=0.6, ampDrive=0.1, ampTone=900, fretNoiseAmt=0, level=-2)
TB('Synthetic', 'Harmonic Chime', True, pickPos=0.5, pickupPos=0.25, pickupBlend=0.5, tone=0.9, decay=1.0, body=0.2, ampDrive=0.1, ampTone=7000, fretNoiseAmt=0.1, fxChorus=0.4, level=-6)
TB('Synthetic', 'Plucked Nylon-ish', pickPos=0.3, pickupPos=0.5, pickupBlend=0.0, tone=0.4, decay=0.7, body=0.9, ampDrive=0.05, ampTone=2500, fretNoiseAmt=0.2, level=-5)
TB('Synthetic', 'Thin Wire', True, pickPos=0.05, pickupPos=0.1, pickupBlend=1.0, tone=1.0, decay=0.9, body=0.1, ampDrive=0.2, ampTone=8000, fretNoiseAmt=0.5, level=-7)
TB('Synthetic', 'Long Sustain', pickPos=0.2, pickupPos=0.3, pickupBlend=0.5, tone=0.6, decay=1.0, body=0.4, ampDrive=0.3, ampTone=3500, fretNoiseAmt=0.3, fxChorus=0.3, level=-6)
TB('Finger', 'Soft Ballad', pickPos=0.3, pickupPos=0.38, pickupBlend=0.3, tone=0.4, decay=0.85, body=0.5, ampDrive=0.1, ampTone=2400, fretNoiseAmt=0.15, velSens=0.9, fxChorus=0.15, level=-5)

# ------------------------------------------------------------------------------------------------ Carapace (16-sound kits)
CA_SOUNDS = ['kick', 'snare', 'rim', 'clap', 'chh', 'phh', 'ohh', 'ltom', 'mtom', 'htom', 'crash', 'ride', 'clave', 'cowbell', 'conga', 'shaker']
CA_FIELDS = {'model': 'Model', 'pitch': 'Pitch', 'penv': 'PitchEnv', 'ptime': 'PitchTime', 'decay': 'Decay', 'tone': 'Tone', 'noise': 'Noise', 'snap': 'Snap', 'drive': 'Drive', 'level': 'Level', 'pan': 'Pan', 'choke': 'Choke'}
def kit(cat, name, adv=False, master=None, **sounds):
    vals = dict(master or {})
    for s, fields in sounds.items():
        n = CA_SOUNDS.index(s) + 1
        for f, v in fields.items(): vals[f's{n}{CA_FIELDS[f]}'] = v
    preset('Carapace', cat, name, adv, None, None, **vals)
K = lambda **k: k
kit('Classic', 'Init Carapace')
kit('Classic', '808 Style', kick=K(model=MEMBRANE, pitch=33, penv=18, ptime=0.03, decay=0.9, tone=0.3, snap=0.2, drive=0.1), snare=K(model=NOISE_M, pitch=62, decay=0.2, tone=0.5, noise=0.7, snap=0.4), clap=K(decay=0.22, tone=0.75, noise=0.95), chh=K(decay=0.05, tone=0.7), ohh=K(decay=0.4, tone=0.7), ltom=K(pitch=43, penv=14, decay=0.5), mtom=K(pitch=48, penv=14, decay=0.45), htom=K(pitch=53, penv=14, decay=0.4), cowbell=K(pitch=76, decay=0.3, tone=0.3), clave=K(pitch=96, decay=0.08), conga=K(pitch=64, decay=0.25), master={'masterTone': 0.45})
kit('Classic', '909 Style', kick=K(model=MEMBRANE, pitch=36, penv=36, ptime=0.035, decay=0.4, tone=0.5, snap=0.5, drive=0.2), snare=K(model=NOISE_M, pitch=65, decay=0.25, tone=0.6, noise=0.6, snap=0.6), clap=K(decay=0.2, tone=0.7, noise=1), chh=K(decay=0.07, tone=0.65, noise=0.5), ohh=K(decay=0.5, tone=0.65, noise=0.5), ride=K(decay=1.4, tone=0.7), crash=K(decay=1.8, tone=0.8), ltom=K(pitch=45, penv=16, decay=0.5), mtom=K(pitch=50, penv=16, decay=0.45), htom=K(pitch=55, penv=16, decay=0.4), master={'masterDrive': 0.1})
kit('Classic', '707 Style', kick=K(pitch=38, penv=24, ptime=0.02, decay=0.25, tone=0.6, snap=0.5), snare=K(pitch=66, decay=0.15, tone=0.65, noise=0.65, snap=0.5), clap=K(decay=0.15, noise=0.9), chh=K(decay=0.04, tone=0.75), ohh=K(decay=0.3, tone=0.75), cowbell=K(pitch=77, decay=0.25, tone=0.25), master={})
kit('Classic', 'Lo-Fi Box', True, kick=K(pitch=38, penv=20, decay=0.3, tone=0.2, drive=0.3), snare=K(pitch=60, decay=0.18, tone=0.35, noise=0.6, drive=0.3), chh=K(decay=0.04, tone=0.4), ohh=K(decay=0.25, tone=0.4), clap=K(decay=0.15, tone=0.4, noise=0.8, drive=0.2), master={'masterTone': 0.25, 'masterDrive': 0.25})
kit('Classic', 'Analog Room', kick=K(pitch=37, penv=28, decay=0.5, tone=0.45, snap=0.4, noise=0.1), snare=K(pitch=63, decay=0.3, tone=0.55, noise=0.65, snap=0.5), chh=K(decay=0.08, tone=0.6), ohh=K(decay=0.6, tone=0.6), ltom=K(decay=0.6), mtom=K(decay=0.55), htom=K(decay=0.5), crash=K(decay=2.0), ride=K(decay=1.6))
# Trap / Drill / Phonk
kit('Trap', 'Trap Kit', kick=K(pitch=34, penv=22, ptime=0.03, decay=0.7, tone=0.35, snap=0.5, drive=0.15), snare=K(pitch=64, decay=0.22, tone=0.65, noise=0.7, snap=0.7), clap=K(decay=0.2, tone=0.8, noise=1, snap=0.6), chh=K(decay=0.035, tone=0.8, noise=0.4), ohh=K(decay=0.35, tone=0.8), rim=K(pitch=88, decay=0.06), master={'masterDrive': 0.05})
kit('Trap', 'Drill Kit', kick=K(pitch=36, penv=30, ptime=0.025, decay=0.35, tone=0.5, snap=0.6, drive=0.2), snare=K(pitch=70, decay=0.18, tone=0.75, noise=0.6, snap=0.8), rim=K(pitch=90, decay=0.05, tone=0.7), chh=K(decay=0.03, tone=0.85), ohh=K(decay=0.25, tone=0.85), clap=K(decay=0.15, noise=1, tone=0.8), master={})
kit('Trap', 'Phonk Kit', True, kick=K(pitch=35, penv=20, decay=0.5, tone=0.3, drive=0.4), snare=K(pitch=60, decay=0.25, tone=0.4, noise=0.7, drive=0.35), cowbell=K(pitch=74, decay=0.35, tone=0.4, drive=0.3), chh=K(decay=0.04, tone=0.5, drive=0.2), ohh=K(decay=0.3, tone=0.5), clap=K(decay=0.2, tone=0.5, noise=0.9, drive=0.3), master={'masterTone': 0.35, 'masterDrive': 0.3})
kit('Trap', 'Rage Kit', True, kick=K(pitch=36, penv=36, ptime=0.02, decay=0.45, tone=0.6, snap=0.7, drive=0.5), snare=K(pitch=66, decay=0.2, tone=0.7, noise=0.6, snap=0.9, drive=0.3), clap=K(decay=0.18, noise=1, tone=0.85, drive=0.3), chh=K(decay=0.03, tone=0.9), ohh=K(decay=0.3, tone=0.9), master={'masterDrive': 0.35})
kit('Trap', 'Plugg Kit', kick=K(pitch=36, penv=16, decay=0.4, tone=0.3, snap=0.3), snare=K(pitch=62, decay=0.15, tone=0.5, noise=0.5, snap=0.5), rim=K(pitch=86, decay=0.08), chh=K(decay=0.03, tone=0.7), ohh=K(decay=0.2, tone=0.7), clave=K(pitch=93, decay=0.07), master={'masterTone': 0.4})
kit('Trap', 'Memphis Kit', True, kick=K(pitch=34, penv=18, decay=0.6, tone=0.25, drive=0.35), snare=K(pitch=61, decay=0.22, tone=0.35, noise=0.75, drive=0.4), chh=K(decay=0.045, tone=0.45, drive=0.2), ohh=K(decay=0.3, tone=0.45), cowbell=K(pitch=75, decay=0.3, tone=0.3, drive=0.3), master={'masterTone': 0.3, 'masterDrive': 0.3})
# House / Techno / Dance
kit('Dance', 'House Kit', kick=K(pitch=37, penv=30, ptime=0.03, decay=0.4, tone=0.5, snap=0.5, drive=0.1), snare=K(pitch=64, decay=0.22, tone=0.6, noise=0.6, snap=0.5), clap=K(decay=0.22, tone=0.7, noise=1), chh=K(decay=0.06, tone=0.7), ohh=K(decay=0.45, tone=0.7), ride=K(decay=1.2), shaker=K(decay=0.08, tone=0.9), master={})
kit('Dance', 'Techno Kit', kick=K(pitch=36, penv=32, ptime=0.03, decay=0.35, tone=0.45, snap=0.6, drive=0.35), snare=K(pitch=65, decay=0.15, tone=0.6, noise=0.7, snap=0.6), clap=K(decay=0.14, noise=1, tone=0.7), chh=K(decay=0.04, tone=0.8, noise=0.5), ohh=K(decay=0.3, tone=0.8), rim=K(pitch=90, decay=0.04), master={'masterDrive': 0.25})
kit('Dance', 'Deep House', kick=K(pitch=35, penv=20, decay=0.5, tone=0.35, snap=0.3), snare=K(pitch=62, decay=0.25, tone=0.45, noise=0.6, snap=0.4), clap=K(decay=0.25, tone=0.6, noise=0.9), chh=K(decay=0.07, tone=0.55), ohh=K(decay=0.5, tone=0.55), shaker=K(decay=0.1, tone=0.8), conga=K(pitch=63, decay=0.3), master={'masterTone': 0.45})
kit('Dance', 'Hard Techno', True, kick=K(pitch=38, penv=40, ptime=0.025, decay=0.3, tone=0.6, snap=0.8, drive=0.7), snare=K(pitch=68, decay=0.12, tone=0.7, noise=0.8, snap=0.8, drive=0.4), chh=K(decay=0.03, tone=0.9, drive=0.3), ohh=K(decay=0.25, tone=0.9), clap=K(decay=0.12, noise=1, drive=0.4), master={'masterDrive': 0.5})
kit('Dance', 'UK Garage', kick=K(pitch=38, penv=26, decay=0.3, tone=0.5, snap=0.5), snare=K(pitch=67, decay=0.18, tone=0.7, noise=0.55, snap=0.7), rim=K(pitch=89, decay=0.05), clap=K(decay=0.16, noise=0.9), chh=K(decay=0.04, tone=0.85), ohh=K(decay=0.28, tone=0.85), shaker=K(decay=0.06, tone=0.95), master={})
kit('Dance', 'Electro Kit', kick=K(pitch=36, penv=34, ptime=0.03, decay=0.35, tone=0.55, snap=0.5, drive=0.15), snare=K(pitch=64, decay=0.2, tone=0.6, noise=0.7, snap=0.6), clap=K(decay=0.2, noise=1), cowbell=K(pitch=76, decay=0.3), clave=K(pitch=96, decay=0.08), chh=K(decay=0.05, tone=0.75), ohh=K(decay=0.4, tone=0.75), master={})
kit('Dance', 'Jungle Kit', True, kick=K(pitch=37, penv=28, decay=0.3, tone=0.5, snap=0.6), snare=K(pitch=68, decay=0.16, tone=0.75, noise=0.5, snap=0.8), rim=K(pitch=88, decay=0.06), chh=K(decay=0.035, tone=0.8), ohh=K(decay=0.25, tone=0.8), ride=K(decay=0.9, tone=0.8), master={'masterTone': 0.6})
kit('Dance', 'Hyperpop Kit', True, kick=K(pitch=38, penv=40, decay=0.25, tone=0.7, snap=0.8, drive=0.5), snare=K(pitch=72, decay=0.15, tone=0.85, noise=0.6, snap=1.0, drive=0.3), clap=K(decay=0.14, noise=1, tone=0.9, drive=0.3), chh=K(decay=0.03, tone=1.0), ohh=K(decay=0.2, tone=1.0), master={'masterTone': 0.7, 'masterDrive': 0.3})
# World / Percussion
kit('Percussion', 'Latin Percussion', kick=K(pitch=40, penv=12, decay=0.3, tone=0.5), snare=K(model=WOOD, pitch=70, decay=0.12), conga=K(pitch=62, penv=6, decay=0.35, tone=0.5), ltom=K(model=TOM, pitch=50, penv=6, decay=0.4), mtom=K(pitch=57, penv=6, decay=0.35), htom=K(pitch=62, penv=6, decay=0.3), cowbell=K(pitch=78, decay=0.25), clave=K(pitch=97, decay=0.08), shaker=K(decay=0.09, tone=0.9), master={})
kit('Percussion', 'Afro Kit', kick=K(pitch=38, penv=14, decay=0.35, tone=0.4), snare=K(model=WOOD, pitch=68, decay=0.15, tone=0.6), rim=K(pitch=85, decay=0.08), conga=K(pitch=60, penv=8, decay=0.4), ltom=K(pitch=48, penv=8, decay=0.45), mtom=K(pitch=55, penv=8, decay=0.4), htom=K(pitch=60, penv=8, decay=0.35), shaker=K(decay=0.1, tone=0.85), clave=K(pitch=95, decay=0.1), master={'masterTone': 0.45})
kit('Percussion', 'Baile Funk', True, kick=K(pitch=36, penv=24, decay=0.3, tone=0.4, drive=0.2), snare=K(pitch=66, decay=0.14, tone=0.6, noise=0.6, snap=0.7), conga=K(pitch=66, penv=10, decay=0.2, tone=0.6), ltom=K(pitch=50, penv=12, decay=0.3), htom=K(pitch=62, penv=12, decay=0.25), cowbell=K(pitch=80, decay=0.2), clave=K(pitch=98, decay=0.07), master={'masterDrive': 0.15})
kit('Percussion', 'Bossa Brush', kick=K(pitch=40, penv=10, decay=0.3, tone=0.3, snap=0.2), snare=K(model=NOISE_M, pitch=62, decay=0.3, tone=0.4, noise=0.95, snap=0.1), rim=K(pitch=84, decay=0.1, tone=0.4), chh=K(decay=0.06, tone=0.5), ohh=K(decay=0.4, tone=0.5), shaker=K(decay=0.12, tone=0.7), ride=K(decay=1.0, tone=0.5), master={'masterTone': 0.4})
kit('Percussion', 'Tabla-ish', True, kick=K(model=TOM, pitch=46, penv=-0 + 10, ptime=0.1, decay=0.4, tone=0.5), snare=K(model=TOM, pitch=66, penv=4, decay=0.15, tone=0.7), conga=K(pitch=64, penv=12, ptime=0.12, decay=0.3), ltom=K(pitch=52, penv=10, ptime=0.15, decay=0.5), htom=K(pitch=70, penv=6, decay=0.2), master={})
kit('Percussion', 'Metal Shop', True, kick=K(model=METAL, pitch=40, decay=0.3, tone=0.3), snare=K(model=METAL, pitch=60, decay=0.25, tone=0.6, noise=0.5), rim=K(model=METAL, pitch=84, decay=0.1), clap=K(model=METAL, pitch=72, decay=0.2, tone=0.8), cowbell=K(model=METAL, pitch=70, decay=0.5, tone=0.4), clave=K(model=METAL, pitch=90, decay=0.15), conga=K(model=METAL, pitch=58, decay=0.4), shaker=K(model=METAL, pitch=95, decay=0.06), master={})
# Hip hop / Boom bap / RnB
kit('Hip Hop', 'Boom Bap', kick=K(pitch=38, penv=20, decay=0.3, tone=0.3, snap=0.4, drive=0.25), snare=K(pitch=63, decay=0.2, tone=0.45, noise=0.7, snap=0.6, drive=0.2), rim=K(pitch=86, decay=0.07), chh=K(decay=0.05, tone=0.5), ohh=K(decay=0.3, tone=0.5), master={'masterTone': 0.35, 'masterDrive': 0.15})
kit('Hip Hop', 'Neo-Soul Kit', kick=K(pitch=38, penv=14, decay=0.35, tone=0.3, snap=0.3), snare=K(pitch=62, decay=0.22, tone=0.4, noise=0.6, snap=0.4), rim=K(pitch=84, decay=0.09, tone=0.4), chh=K(decay=0.06, tone=0.45), ohh=K(decay=0.35, tone=0.45), shaker=K(decay=0.1, tone=0.7), ride=K(decay=1.0, tone=0.5), master={'masterTone': 0.35})
kit('Hip Hop', 'Gospel Kit', kick=K(pitch=36, penv=24, decay=0.4, tone=0.45, snap=0.5), snare=K(pitch=66, decay=0.25, tone=0.6, noise=0.6, snap=0.7), ltom=K(decay=0.6), mtom=K(decay=0.55), htom=K(decay=0.5), crash=K(decay=2.2), ride=K(decay=1.6, tone=0.7), chh=K(decay=0.07, tone=0.65), ohh=K(decay=0.5, tone=0.65), master={})
kit('Hip Hop', 'R&B Smooth', kick=K(pitch=36, penv=16, decay=0.45, tone=0.3, snap=0.25), snare=K(pitch=62, decay=0.2, tone=0.5, noise=0.5, snap=0.5), clap=K(decay=0.25, tone=0.55, noise=0.9), rim=K(pitch=85, decay=0.08), chh=K(decay=0.05, tone=0.55), ohh=K(decay=0.3, tone=0.55), shaker=K(decay=0.08, tone=0.8), master={'masterTone': 0.4})
kit('Hip Hop', 'Dusty Grime', True, kick=K(pitch=37, penv=26, decay=0.3, tone=0.4, drive=0.3), snare=K(pitch=68, decay=0.16, tone=0.65, noise=0.6, snap=0.8, drive=0.25), clap=K(decay=0.14, noise=1, tone=0.7), rim=K(pitch=90, decay=0.05), chh=K(decay=0.035, tone=0.75), ohh=K(decay=0.25, tone=0.75), master={'masterDrive': 0.2})
# Acoustic-leaning
kit('Acoustic', 'Rock Kit', kick=K(pitch=38, penv=30, ptime=0.04, decay=0.35, tone=0.5, noise=0.15, snap=0.6), snare=K(pitch=64, decay=0.28, tone=0.6, noise=0.7, snap=0.6), ltom=K(pitch=46, penv=12, decay=0.7), mtom=K(pitch=51, penv=12, decay=0.6), htom=K(pitch=56, penv=12, decay=0.5), crash=K(decay=2.5, tone=0.8), ride=K(decay=1.8, tone=0.7), chh=K(decay=0.08, tone=0.65, noise=0.5), ohh=K(decay=0.7, tone=0.65), master={})
kit('Acoustic', 'Jazz Kit', kick=K(pitch=42, penv=14, decay=0.3, tone=0.35, snap=0.3), snare=K(pitch=66, decay=0.22, tone=0.55, noise=0.75, snap=0.4), ltom=K(pitch=50, penv=8, decay=0.5), htom=K(pitch=58, penv=8, decay=0.4), ride=K(decay=2.0, tone=0.6, noise=0.3), crash=K(decay=2.0, tone=0.7), chh=K(decay=0.07, tone=0.55), ohh=K(decay=0.5, tone=0.55), master={'masterTone': 0.45})
kit('Acoustic', 'Tight Pop', kick=K(pitch=38, penv=28, decay=0.3, tone=0.5, snap=0.6, drive=0.1), snare=K(pitch=65, decay=0.2, tone=0.65, noise=0.65, snap=0.7), clap=K(decay=0.18, noise=1), chh=K(decay=0.05, tone=0.7), ohh=K(decay=0.35, tone=0.7), shaker=K(decay=0.07, tone=0.9), master={})
kit('Acoustic', 'Big Room Toms', True, kick=K(pitch=36, penv=30, decay=0.6, tone=0.45, snap=0.6), snare=K(pitch=63, decay=0.4, tone=0.55, noise=0.7, snap=0.6), ltom=K(pitch=44, penv=14, decay=1.0), mtom=K(pitch=49, penv=14, decay=0.9), htom=K(pitch=54, penv=14, decay=0.8), crash=K(decay=3.0), master={})
# FX / Experimental
kit('FX', 'Glitch Kit', True, kick=K(model=NOISE_M, pitch=40, decay=0.08, tone=0.3, noise=0.5, snap=1.0), snare=K(model=METAL, pitch=80, decay=0.06, tone=0.9), rim=K(model=WOOD, pitch=100, decay=0.03), clap=K(decay=0.05, noise=1, snap=1), chh=K(decay=0.015, tone=1.0), ohh=K(decay=0.1, tone=1.0), clave=K(model=METAL, pitch=108, decay=0.04), master={'masterDrive': 0.2})
kit('FX', 'Industrial', True, kick=K(pitch=36, penv=36, decay=0.4, tone=0.5, snap=0.8, drive=0.8), snare=K(model=METAL, pitch=62, decay=0.3, tone=0.6, noise=0.7, drive=0.5), clap=K(decay=0.2, noise=1, drive=0.5), chh=K(decay=0.04, tone=0.9, drive=0.4), ohh=K(decay=0.35, tone=0.9, drive=0.4), cowbell=K(model=METAL, pitch=70, decay=0.4, drive=0.5), master={'masterDrive': 0.6})
kit('FX', 'Soft Pads Kit', True, kick=K(pitch=36, penv=10, decay=0.8, tone=0.2, snap=0.1), snare=K(model=NOISE_M, pitch=60, decay=0.5, tone=0.3, noise=0.9, snap=0.1), chh=K(decay=0.15, tone=0.3), ohh=K(decay=0.8, tone=0.3), clap=K(decay=0.4, tone=0.4, noise=1, snap=0.1), master={'masterTone': 0.2})
kit('FX', 'Wood Kit', True, kick=K(model=WOOD, pitch=48, decay=0.3, tone=0.2), snare=K(model=WOOD, pitch=70, decay=0.15, tone=0.5), rim=K(model=WOOD, pitch=90, decay=0.08), clap=K(model=WOOD, pitch=80, decay=0.12, tone=0.6), chh=K(model=WOOD, pitch=100, decay=0.04), ohh=K(model=WOOD, pitch=100, decay=0.15), ltom=K(model=WOOD, pitch=55, decay=0.3), mtom=K(model=WOOD, pitch=62, decay=0.25), htom=K(model=WOOD, pitch=69, decay=0.2), master={})
kit('FX', 'Tom Machine', True, kick=K(model=TOM, pitch=38, penv=20, decay=0.6), snare=K(model=TOM, pitch=60, penv=10, decay=0.3), rim=K(model=TOM, pitch=72, penv=6, decay=0.15), clap=K(model=TOM, pitch=66, penv=8, decay=0.2), chh=K(model=TOM, pitch=84, penv=4, decay=0.08), ohh=K(model=TOM, pitch=84, penv=4, decay=0.3), conga=K(model=TOM, pitch=64, penv=10, decay=0.3), master={})

# ------------------------------------------------------------------------------------------------ Live Drum Synth (12 classic two-osc pieces)
LD_SOUNDS = ['kick', 'snare', 'rim', 'clap', 'chh', 'ohh', 'ltom', 'htom', 'ride', 'crash', 'perc', 'zap']
LD_FIELDS = {'w1': 'O1Wave', 'p1': 'O1Pitch', 'pe': 'O1PitchEnv', 'pt': 'PitchTime', 'd1': 'O1Decay', 'w2': 'O2Wave', 'p2': 'O2Pitch', 'l2': 'O2Level', 'd2': 'O2Decay', 'nz': 'NzLevel', 'nc': 'NzColour', 'nd': 'NzDecay', 'level': 'Level', 'pan': 'Pan'}
def ldkit(cat, name, adv=False, master=None, **sounds):
    vals = dict(master or {})
    for s, fields in sounds.items():
        n = LD_SOUNDS.index(s) + 1
        for f, v in fields.items(): vals[f's{n}{LD_FIELDS[f]}'] = v
    preset('Live Drum Synth', cat, name, adv, None, None, **vals)
ldkit('Kits', 'Init Live Drums')
ldkit('Kits', 'Analog Kit', kick=K(w1=D_SINE, p1=52, pe=36, pt=0.03, d1=0.45, nz=0.1, nd=0.02), snare=K(w1=D_TRI, p1=190, pe=12, d1=0.2, w2=D_SINE, p2=330, l2=0.3, d2=0.1, nz=0.6, nc=0.6, nd=0.18), chh=K(w1=D_SQR, p1=3200, d1=0.04, w2=D_SQR, p2=4700, l2=0.6, d2=0.04, nz=0.5, nc=0.9, nd=0.05), ohh=K(w1=D_SQR, p1=3200, d1=0.3, w2=D_SQR, p2=4700, l2=0.6, d2=0.3, nz=0.5, nc=0.9, nd=0.35), clap=K(w1=D_NOISE, d1=0.15, nz=1, nc=0.7, nd=0.18), ltom=K(w1=D_SINE, p1=110, pe=14, pt=0.08, d1=0.5), htom=K(w1=D_SINE, p1=180, pe=14, pt=0.08, d1=0.4))
ldkit('Kits', 'Tight Studio Kit', kick=K(w1=D_SINE, p1=58, pe=30, pt=0.025, d1=0.3, nz=0.15, nc=0.4, nd=0.01), snare=K(w1=D_TRI, p1=210, pe=10, d1=0.15, nz=0.7, nc=0.65, nd=0.14), chh=K(w1=D_SQR, p1=3600, d1=0.03, nz=0.6, nc=0.95, nd=0.04), ohh=K(w1=D_SQR, p1=3600, d1=0.22, nz=0.6, nc=0.95, nd=0.25), clap=K(w1=D_NOISE, d1=0.12, nz=1, nc=0.75, nd=0.14), rim=K(w1=D_TRI, p1=900, d1=0.04, nz=0.2, nd=0.02))
ldkit('Kits', 'Big Live Kit', kick=K(w1=D_SINE, p1=48, pe=30, pt=0.05, d1=0.7, w2=D_SINE, p2=95, l2=0.2, d2=0.2, nz=0.2, nc=0.3, nd=0.03), snare=K(w1=D_TRI, p1=180, pe=14, d1=0.3, w2=D_SINE, p2=300, l2=0.3, d2=0.15, nz=0.7, nc=0.55, nd=0.3), ltom=K(w1=D_SINE, p1=95, pe=16, pt=0.1, d1=0.9), htom=K(w1=D_SINE, p1=160, pe=16, pt=0.1, d1=0.7), crash=K(w1=D_SQR, p1=5200, d1=2.5, nz=0.9, nc=0.8, nd=2.5), ride=K(w1=D_SQR, p1=4100, d1=1.8, nz=0.6, nc=0.75, nd=1.6))
ldkit('Kits', 'Dead Room Kit', kick=K(w1=D_SINE, p1=55, pe=24, d1=0.25, nz=0.1, nc=0.3, nd=0.01), snare=K(w1=D_TRI, p1=200, pe=8, d1=0.12, nz=0.6, nc=0.5, nd=0.1), chh=K(w1=D_SQR, p1=3000, d1=0.025, nz=0.5, nc=0.85, nd=0.03), ohh=K(w1=D_SQR, p1=3000, d1=0.15, nz=0.5, nc=0.85, nd=0.18), ltom=K(w1=D_SINE, p1=105, pe=12, d1=0.3), htom=K(w1=D_SINE, p1=175, pe=12, d1=0.25), master={'masterDrive': 0.05})
ldkit('Kits', 'Vintage Drum Box', kick=K(w1=D_SINE, p1=50, pe=20, d1=0.35, nz=0.05), snare=K(w1=D_SINE, p1=240, pe=6, d1=0.12, nz=0.5, nc=0.6, nd=0.12), rim=K(w1=D_SQR, p1=1100, d1=0.03), clap=K(w1=D_NOISE, d1=0.1, nz=0.9, nc=0.6, nd=0.12), chh=K(w1=D_SQR, p1=4000, d1=0.03, nz=0.4, nc=0.9, nd=0.03), ohh=K(w1=D_SQR, p1=4000, d1=0.2, nz=0.4, nc=0.9, nd=0.22), perc=K(w1=D_TRI, p1=800, pe=12, d1=0.12), master={'masterDrive': 0.1})
ldkit('Kits', 'Dub Kit', kick=K(w1=D_SINE, p1=45, pe=16, pt=0.05, d1=0.6, nz=0.05), snare=K(w1=D_TRI, p1=170, pe=10, d1=0.25, nz=0.5, nc=0.45, nd=0.3), rim=K(w1=D_TRI, p1=700, d1=0.08, nz=0.2, nd=0.04), chh=K(w1=D_SQR, p1=2800, d1=0.05, nz=0.5, nc=0.8, nd=0.06), ohh=K(w1=D_SQR, p1=2800, d1=0.35, nz=0.5, nc=0.8, nd=0.4))
ldkit('Kits', 'Garage Kit', kick=K(w1=D_SINE, p1=60, pe=28, d1=0.25, nz=0.1, nd=0.01), snare=K(w1=D_TRI, p1=230, pe=12, d1=0.14, nz=0.6, nc=0.75, nd=0.12), rim=K(w1=D_SQR, p1=1300, d1=0.03, nz=0.3, nc=0.9, nd=0.02), clap=K(w1=D_NOISE, d1=0.1, nz=1, nc=0.8, nd=0.12), chh=K(w1=D_SQR, p1=4200, d1=0.025, nz=0.6, nc=1, nd=0.03), ohh=K(w1=D_SQR, p1=4200, d1=0.18, nz=0.6, nc=1, nd=0.2), perc=K(w1=D_NOISE, d1=0.05, nz=0.8, nc=0.9, nd=0.05))
ldkit('Kits', 'Grime Kit', True, kick=K(w1=D_SINE, p1=50, pe=34, d1=0.3, nz=0.15, nd=0.01), snare=K(w1=D_SQR, p1=260, pe=14, d1=0.12, nz=0.7, nc=0.8, nd=0.1), rim=K(w1=D_SQR, p1=1500, d1=0.03), clap=K(w1=D_NOISE, d1=0.08, nz=1, nc=0.85, nd=0.1), chh=K(w1=D_SQR, p1=4500, d1=0.02, nz=0.6, nc=1, nd=0.02), zap=K(w1=D_SAW, p1=1200, pe=48, pt=0.08, d1=0.2), master={'masterDrive': 0.2})
ldkit('Kits', 'Bossa Kit', kick=K(w1=D_SINE, p1=62, pe=10, d1=0.25, nz=0.05), snare=K(w1=D_NOISE, d1=0.25, nz=1, nc=0.5, nd=0.25), rim=K(w1=D_TRI, p1=850, d1=0.06, nz=0.15, nd=0.03), chh=K(w1=D_SQR, p1=3300, d1=0.04, nz=0.5, nc=0.8, nd=0.05), ohh=K(w1=D_SQR, p1=3300, d1=0.25, nz=0.5, nc=0.8, nd=0.3), ride=K(w1=D_SQR, p1=4000, d1=1.2, nz=0.5, nc=0.7, nd=1.0), perc=K(w1=D_NOISE, d1=0.07, nz=0.7, nc=0.85, nd=0.07))
ldkit('Kits', 'Gospel Live Kit', kick=K(w1=D_SINE, p1=52, pe=30, d1=0.4, nz=0.15, nd=0.02), snare=K(w1=D_TRI, p1=200, pe=14, d1=0.25, w2=D_SINE, p2=340, l2=0.3, d2=0.12, nz=0.65, nc=0.6, nd=0.25), ltom=K(w1=D_SINE, p1=100, pe=14, d1=0.6), htom=K(w1=D_SINE, p1=170, pe=14, d1=0.5), crash=K(w1=D_SQR, p1=5000, d1=2.2, nz=0.9, nc=0.8, nd=2.2), ride=K(w1=D_SQR, p1=4200, d1=1.6, nz=0.6, nc=0.75, nd=1.4), chh=K(w1=D_SQR, p1=3400, d1=0.06, nz=0.5, nc=0.9, nd=0.07), ohh=K(w1=D_SQR, p1=3400, d1=0.5, nz=0.5, nc=0.9, nd=0.5))
ldkit('Kits', 'Neo-Soul Live', kick=K(w1=D_SINE, p1=50, pe=16, d1=0.35, nz=0.08), snare=K(w1=D_TRI, p1=185, pe=8, d1=0.2, nz=0.55, nc=0.45, nd=0.2), rim=K(w1=D_TRI, p1=780, d1=0.08, nz=0.2, nd=0.04), chh=K(w1=D_SQR, p1=3000, d1=0.05, nz=0.45, nc=0.75, nd=0.06), ohh=K(w1=D_SQR, p1=3000, d1=0.3, nz=0.45, nc=0.75, nd=0.35), perc=K(w1=D_NOISE, d1=0.09, nz=0.6, nc=0.7, nd=0.09), ride=K(w1=D_SQR, p1=3800, d1=1.0, nz=0.5, nc=0.65, nd=0.9))
ldkit('Kits', 'Electro Pop Kit', kick=K(w1=D_SINE, p1=55, pe=36, pt=0.03, d1=0.35, nz=0.15, nc=0.5, nd=0.01), snare=K(w1=D_SQR, p1=220, pe=16, d1=0.18, nz=0.7, nc=0.7, nd=0.16), clap=K(w1=D_NOISE, d1=0.14, nz=1, nc=0.75, nd=0.16), chh=K(w1=D_SQR, p1=3800, d1=0.03, nz=0.6, nc=0.95, nd=0.04), ohh=K(w1=D_SQR, p1=3800, d1=0.3, nz=0.6, nc=0.95, nd=0.3), zap=K(w1=D_SAW, p1=900, pe=36, pt=0.1, d1=0.25), perc=K(w1=D_SINE, p1=600, pe=24, pt=0.05, d1=0.15))
ldkit('Kits', 'Breakbeat Kit', kick=K(w1=D_SINE, p1=56, pe=26, d1=0.28, nz=0.15, nc=0.4, nd=0.02), snare=K(w1=D_TRI, p1=215, pe=14, d1=0.18, nz=0.75, nc=0.7, nd=0.16), rim=K(w1=D_TRI, p1=950, d1=0.04), chh=K(w1=D_SQR, p1=3500, d1=0.035, nz=0.55, nc=0.9, nd=0.04), ohh=K(w1=D_SQR, p1=3500, d1=0.25, nz=0.55, nc=0.9, nd=0.28), ride=K(w1=D_SQR, p1=4000, d1=0.9, nz=0.5, nc=0.75, nd=0.8), master={'masterDrive': 0.15})
ldkit('Kits', 'Minimal Kit', kick=K(w1=D_SINE, p1=50, pe=20, d1=0.2, nz=0), snare=K(w1=D_SINE, p1=250, pe=4, d1=0.08, nz=0.4, nc=0.7, nd=0.06), rim=K(w1=D_SQR, p1=1800, d1=0.02), clap=K(w1=D_NOISE, d1=0.06, nz=0.8, nc=0.8, nd=0.06), chh=K(w1=D_SQR, p1=5000, d1=0.015, nz=0.4, nc=1, nd=0.015), ohh=K(w1=D_SQR, p1=5000, d1=0.1, nz=0.4, nc=1, nd=0.12), perc=K(w1=D_SINE, p1=1200, d1=0.05))
ldkit('Kits', 'Trip Hop Kit', True, kick=K(w1=D_SINE, p1=46, pe=18, pt=0.05, d1=0.5, nz=0.1, nc=0.2, nd=0.03), snare=K(w1=D_TRI, p1=175, pe=8, d1=0.25, nz=0.6, nc=0.4, nd=0.25), chh=K(w1=D_SQR, p1=2600, d1=0.06, nz=0.5, nc=0.6, nd=0.07), ohh=K(w1=D_SQR, p1=2600, d1=0.4, nz=0.5, nc=0.6, nd=0.45), perc=K(w1=D_TRI, p1=500, pe=10, d1=0.2), master={'masterDrive': 0.3})
ldkit('Kits', 'Synthwave Kit', kick=K(w1=D_SINE, p1=54, pe=30, d1=0.4, w2=D_SINE, p2=108, l2=0.15, d2=0.1, nz=0.15, nc=0.5, nd=0.015), snare=K(w1=D_TRI, p1=200, pe=12, d1=0.35, nz=0.7, nc=0.6, nd=0.4), clap=K(w1=D_NOISE, d1=0.3, nz=1, nc=0.65, nd=0.35), chh=K(w1=D_SQR, p1=3400, d1=0.04, nz=0.5, nc=0.9, nd=0.05), ohh=K(w1=D_SQR, p1=3400, d1=0.35, nz=0.5, nc=0.9, nd=0.4), ltom=K(w1=D_SINE, p1=100, pe=30, pt=0.1, d1=0.6), htom=K(w1=D_SINE, p1=170, pe=30, pt=0.1, d1=0.5), zap=K(w1=D_SAW, p1=1500, pe=48, pt=0.15, d1=0.3))
ldkit('Kits', 'Drum & Bass Kit', kick=K(w1=D_SINE, p1=58, pe=30, d1=0.22, nz=0.2, nc=0.5, nd=0.01), snare=K(w1=D_SQR, p1=240, pe=16, d1=0.12, w2=D_TRI, p2=400, l2=0.3, d2=0.06, nz=0.7, nc=0.85, nd=0.1), chh=K(w1=D_SQR, p1=4200, d1=0.02, nz=0.6, nc=1, nd=0.025), ohh=K(w1=D_SQR, p1=4200, d1=0.15, nz=0.6, nc=1, nd=0.18), ride=K(w1=D_SQR, p1=4300, d1=0.7, nz=0.5, nc=0.8, nd=0.6), master={'masterDrive': 0.2})
ldkit('Kits', 'Hyperpop Live', True, kick=K(w1=D_SINE, p1=60, pe=40, d1=0.22, nz=0.25, nc=0.6, nd=0.01), snare=K(w1=D_SQR, p1=280, pe=20, d1=0.1, nz=0.8, nc=0.9, nd=0.08), clap=K(w1=D_NOISE, d1=0.08, nz=1, nc=0.9, nd=0.1), chh=K(w1=D_SQR, p1=5200, d1=0.015, nz=0.7, nc=1, nd=0.02), zap=K(w1=D_SAW, p1=2000, pe=60, pt=0.05, d1=0.15), master={'masterDrive': 0.4})
ldkit('Kits', 'Afrobeat Live', kick=K(w1=D_SINE, p1=54, pe=16, d1=0.3, nz=0.08), snare=K(w1=D_TRI, p1=210, pe=8, d1=0.15, nz=0.5, nc=0.6, nd=0.12), rim=K(w1=D_TRI, p1=820, d1=0.07, nz=0.15, nd=0.04), ltom=K(w1=D_SINE, p1=130, pe=10, pt=0.08, d1=0.35), htom=K(w1=D_SINE, p1=200, pe=10, pt=0.08, d1=0.3), perc=K(w1=D_NOISE, d1=0.08, nz=0.7, nc=0.85, nd=0.08), chh=K(w1=D_SQR, p1=3200, d1=0.04, nz=0.5, nc=0.85, nd=0.05), ohh=K(w1=D_SQR, p1=3200, d1=0.2, nz=0.5, nc=0.85, nd=0.25))
ldkit('Kits', 'Reggaeton Live', kick=K(w1=D_SINE, p1=52, pe=22, d1=0.35, nz=0.1, nd=0.015), snare=K(w1=D_TRI, p1=230, pe=12, d1=0.14, nz=0.6, nc=0.7, nd=0.12), rim=K(w1=D_SQR, p1=1200, d1=0.035, nz=0.2, nc=0.9, nd=0.02), clap=K(w1=D_NOISE, d1=0.1, nz=1, nc=0.75, nd=0.12), perc=K(w1=D_TRI, p1=650, pe=14, d1=0.12), chh=K(w1=D_SQR, p1=3600, d1=0.03, nz=0.55, nc=0.9, nd=0.035), ohh=K(w1=D_SQR, p1=3600, d1=0.2, nz=0.55, nc=0.9, nd=0.22))
# Single-piece focus presets (same kit, one piece dialled for a style)
ldkit('Kicks', 'Deep Kick Kit', kick=K(w1=D_SINE, p1=44, pe=20, pt=0.06, d1=0.8, nz=0.05, nd=0.02))
ldkit('Kicks', 'Punchy Kick Kit', kick=K(w1=D_SINE, p1=58, pe=36, pt=0.02, d1=0.25, nz=0.2, nc=0.5, nd=0.01))
ldkit('Kicks', 'Clicky Kick Kit', kick=K(w1=D_TRI, p1=55, pe=40, pt=0.015, d1=0.3, w2=D_SQR, p2=2500, l2=0.15, d2=0.01, nz=0.3, nc=0.7, nd=0.008))
ldkit('Kicks', 'Long Boom Kit', kick=K(w1=D_SINE, p1=40, pe=14, pt=0.08, d1=1.4, nz=0.05, nd=0.02))
ldkit('Kicks', 'Hard Kick Kit', kick=K(w1=D_SINE, p1=56, pe=40, pt=0.03, d1=0.35, w2=D_SQR, p2=110, l2=0.25, d2=0.08, nz=0.2, nc=0.5, nd=0.01), master={'masterDrive': 0.4})
ldkit('Snares', 'Fat Snare Kit', snare=K(w1=D_TRI, p1=175, pe=14, d1=0.3, w2=D_SINE, p2=300, l2=0.35, d2=0.15, nz=0.7, nc=0.55, nd=0.3))
ldkit('Snares', 'Crack Snare Kit', snare=K(w1=D_SQR, p1=260, pe=20, d1=0.1, w2=D_TRI, p2=450, l2=0.3, d2=0.05, nz=0.8, nc=0.9, nd=0.09))
ldkit('Snares', 'Piccolo Snare Kit', snare=K(w1=D_TRI, p1=300, pe=12, d1=0.1, nz=0.7, nc=0.85, nd=0.1))
ldkit('Snares', 'Brush Snare Kit', snare=K(w1=D_NOISE, d1=0.3, nz=1, nc=0.45, nd=0.3))
ldkit('Snares', 'Rimshot Kit', snare=K(w1=D_TRI, p1=220, pe=10, d1=0.15, w2=D_SQR, p2=1600, l2=0.3, d2=0.02, nz=0.5, nc=0.7, nd=0.12))
ldkit('Hats', 'Bright Hats Kit', chh=K(w1=D_SQR, p1=4600, d1=0.03, w2=D_SQR, p2=6900, l2=0.6, d2=0.03, nz=0.6, nc=1, nd=0.035), ohh=K(w1=D_SQR, p1=4600, d1=0.3, w2=D_SQR, p2=6900, l2=0.6, d2=0.3, nz=0.6, nc=1, nd=0.35))
ldkit('Hats', 'Dark Hats Kit', chh=K(w1=D_SQR, p1=2400, d1=0.05, nz=0.4, nc=0.6, nd=0.06), ohh=K(w1=D_SQR, p1=2400, d1=0.4, nz=0.4, nc=0.6, nd=0.45))
ldkit('Hats', 'Trap Hats Kit', chh=K(w1=D_SQR, p1=4000, d1=0.02, nz=0.7, nc=1, nd=0.02), ohh=K(w1=D_SQR, p1=4000, d1=0.2, nz=0.7, nc=1, nd=0.25))
ldkit('Hats', 'Sizzle Ride Kit', ride=K(w1=D_SQR, p1=4300, d1=2.5, w2=D_SQR, p2=6100, l2=0.5, d2=2.0, nz=0.7, nc=0.8, nd=2.5))
ldkit('Hats', 'Washy Crash Kit', crash=K(w1=D_SQR, p1=5500, d1=3.5, w2=D_SQR, p2=7700, l2=0.5, d2=3.0, nz=1, nc=0.85, nd=3.5))
ldkit('Toms', 'Rototoms Kit', ltom=K(w1=D_SINE, p1=120, pe=24, pt=0.15, d1=0.5), htom=K(w1=D_SINE, p1=200, pe=24, pt=0.15, d1=0.4))
ldkit('Toms', 'Floor Tom Kit', ltom=K(w1=D_SINE, p1=80, pe=12, pt=0.1, d1=1.0, nz=0.1, nc=0.3, nd=0.05), htom=K(w1=D_SINE, p1=130, pe=12, pt=0.1, d1=0.8, nz=0.1, nc=0.3, nd=0.05))
ldkit('Toms', 'Disco Toms Kit', ltom=K(w1=D_SINE, p1=100, pe=30, pt=0.1, d1=0.6), htom=K(w1=D_SINE, p1=170, pe=30, pt=0.1, d1=0.5))
ldkit('Perc', 'Zaps and Blips', zap=K(w1=D_SAW, p1=1800, pe=60, pt=0.06, d1=0.18), perc=K(w1=D_SINE, p1=900, pe=36, pt=0.04, d1=0.1))
ldkit('Perc', 'Cowbell Perc Kit', perc=K(w1=D_SQR, p1=560, d1=0.3, w2=D_SQR, p2=845, l2=0.8, d2=0.3, nz=0.05))

# ------------------------------------------------------------------------------------------------ Drum Pad Model (one modelled drum, keytracked)
DP = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Drum Pad Model', cat, name, adv, props, mods, **k)
DP('Membrane', 'Init Pad', model=MEMBRANE, pitch=48, decay=0.4)
DP('Membrane', 'Deep Kick', model=MEMBRANE, pitch=36, pitchEnv=20, pitchTime=0.04, decay=0.8, tone=0.3, snap=0.3, keyTrack=0.5, level=-3)
DP('Membrane', 'Punch Kick', model=MEMBRANE, pitch=38, pitchEnv=36, pitchTime=0.03, decay=0.35, tone=0.5, snap=0.6, drive=0.15, keyTrack=0.5, level=-3)
DP('Membrane', 'Sub Boom', model=MEMBRANE, pitch=33, pitchEnv=12, pitchTime=0.06, decay=1.4, tone=0.2, snap=0.1, keyTrack=1, level=-2)
DP('Membrane', 'Snare Body', model=MEMBRANE, pitch=60, pitchEnv=10, pitchTime=0.02, decay=0.25, tone=0.6, noise=0.5, snap=0.6, keyTrack=1, level=-4)
DP('Membrane', 'Bongo', model=MEMBRANE, pitch=67, pitchEnv=8, pitchTime=0.03, decay=0.2, tone=0.6, snap=0.4, keyTrack=1, level=-4)
DP('Membrane', 'Taiko', model=MEMBRANE, pitch=40, pitchEnv=10, pitchTime=0.08, decay=1.2, tone=0.35, noise=0.15, snap=0.5, keyTrack=1, level=-3)
DP('Membrane', 'Frame Drum', model=MEMBRANE, pitch=45, pitchEnv=6, pitchTime=0.05, decay=0.6, tone=0.3, noise=0.1, snap=0.3, keyTrack=1, level=-4)
DP('Membrane', 'Melodic Kick', model=MEMBRANE, pitch=48, pitchEnv=24, pitchTime=0.03, decay=0.5, tone=0.4, snap=0.4, keyTrack=1, level=-3)
DP('Tom', 'Rack Tom', model=TOM, pitch=52, pitchEnv=12, pitchTime=0.08, decay=0.5, tone=0.5, snap=0.2, keyTrack=1, level=-4)
DP('Tom', 'Floor Tom', model=TOM, pitch=43, pitchEnv=10, pitchTime=0.1, decay=0.9, tone=0.4, snap=0.2, keyTrack=1, level=-3)
DP('Tom', 'Syndrum', model=TOM, pitch=55, pitchEnv=36, pitchTime=0.15, decay=0.6, tone=0.5, snap=0.1, keyTrack=1, level=-4)
DP('Tom', 'Rototom', model=TOM, pitch=58, pitchEnv=20, pitchTime=0.12, decay=0.45, tone=0.55, keyTrack=1, level=-4)
DP('Tom', 'Timbale', model=TOM, pitch=64, pitchEnv=6, pitchTime=0.03, decay=0.3, tone=0.7, noise=0.1, snap=0.5, keyTrack=1, level=-4)
DP('Tom', 'Conga Tone', model=TOM, pitch=62, pitchEnv=4, pitchTime=0.05, decay=0.3, tone=0.5, snap=0.3, keyTrack=1, level=-4)
DP('Tom', 'Tabla Bayan', True, model=TOM, pitch=46, pitchEnv=12, pitchTime=0.15, decay=0.4, tone=0.5, snap=0.2, keyTrack=1, level=-4)
DP('Metal', 'Closed Hat', model=METAL, pitch=72, decay=0.06, tone=0.6, noise=0.3, snap=0.3, keyTrack=0.5, level=-6)
DP('Metal', 'Open Hat', model=METAL, pitch=72, decay=0.6, tone=0.6, noise=0.3, snap=0.3, keyTrack=0.5, level=-6)
DP('Metal', 'Ride Bell', model=METAL, pitch=79, decay=1.2, tone=0.65, noise=0.2, snap=0.2, keyTrack=1, level=-6)
DP('Metal', 'Crash', model=METAL, pitch=76, decay=1.8, tone=0.75, noise=0.5, snap=0.3, keyTrack=0.5, level=-7)
DP('Metal', 'Cowbell', model=METAL, pitch=76, decay=0.3, tone=0.3, noise=0.05, snap=0.3, drive=0.1, keyTrack=1, level=-5)
DP('Metal', 'Agogo', model=METAL, pitch=84, decay=0.25, tone=0.4, noise=0.05, snap=0.3, keyTrack=1, level=-5)
DP('Metal', 'Gong', True, model=METAL, pitch=55, decay=3.0, tone=0.4, noise=0.2, snap=0.1, keyTrack=1, level=-6)
DP('Metal', 'Triangle', model=METAL, pitch=100, decay=1.5, tone=0.9, noise=0.05, snap=0.2, keyTrack=1, level=-7)
DP('Metal', 'Tambourine', model=METAL, pitch=90, decay=0.15, tone=0.8, noise=0.6, snap=0.5, keyTrack=0.3, level=-6)
DP('Noise', 'Clap', model=NOISE_M, pitch=72, decay=0.18, tone=0.7, noise=0.95, snap=0.6, keyTrack=0.3, level=-5)
DP('Noise', 'Snare Noise', model=NOISE_M, pitch=62, decay=0.22, tone=0.55, noise=0.6, snap=0.5, keyTrack=1, level=-4)
DP('Noise', 'Shaker', model=NOISE_M, pitch=90, decay=0.08, tone=0.9, noise=1, snap=0.2, keyTrack=0.2, level=-6)
DP('Noise', 'Brush', model=NOISE_M, pitch=60, decay=0.35, tone=0.4, noise=1, snap=0.1, keyTrack=0, level=-5)
DP('Noise', 'White Burst', True, model=NOISE_M, pitch=80, decay=0.5, tone=1, noise=1, snap=0.8, keyTrack=0, level=-6)
DP('Noise', 'Noise Sweep', True, model=NOISE_M, pitch=50, decay=1.5, tone=0.3, noise=1, snap=0, keyTrack=1, level=-6)
DP('Wood', 'Rimshot', model=WOOD, pitch=84, decay=0.08, tone=0.5, noise=0.2, snap=0.4, keyTrack=1, level=-4)
DP('Wood', 'Clave', model=WOOD, pitch=95, decay=0.1, tone=0.8, noise=0.1, snap=0.3, keyTrack=1, level=-5)
DP('Wood', 'Wood Block', model=WOOD, pitch=88, decay=0.12, tone=0.6, noise=0.1, snap=0.3, keyTrack=1, level=-4)
DP('Wood', 'Temple Block', model=WOOD, pitch=72, decay=0.25, tone=0.4, noise=0.1, snap=0.2, keyTrack=1, level=-4)
DP('Wood', 'Log Drum', model=WOOD, pitch=60, decay=0.35, tone=0.3, noise=0.05, snap=0.2, keyTrack=1, level=-4)
DP('Wood', 'Castanet', model=WOOD, pitch=100, decay=0.05, tone=0.7, noise=0.3, snap=0.5, keyTrack=1, level=-5)
DP('Wood', 'Marimba Hit', True, model=WOOD, pitch=67, decay=0.4, tone=0.35, noise=0, snap=0.1, keyTrack=1, level=-4)
DP('FX', 'Laser Pad', True, model=MEMBRANE, pitch=84, pitchEnv=60, pitchTime=0.15, decay=0.4, tone=0.8, snap=0.2, keyTrack=1, level=-5)
DP('FX', 'Zap Pad', True, model=MEMBRANE, pitch=72, pitchEnv=48, pitchTime=0.05, decay=0.15, tone=0.9, snap=0.5, drive=0.3, keyTrack=1, level=-5)

# ------------------------------------------------------------------------------------------------ Thump Live (kick with drawable envelopes)
TL = lambda cat, name, adv=False, props=None, mods=None, **k: preset('Thump Live', cat, name, adv, props, mods, **k)
def env(prefix, t, v):
    d = {f'{prefix}T{i+1}': t[i] for i in range(3)}
    d.update({f'{prefix}V{i}': v[i] for i in range(4)})
    return d
def pattern(steps, rate=1):
    d = {'patOn': 1, 'patRate': rate}
    for i in range(16): d[f'pat{i+1}'] = steps[i % len(steps)]
    return d
P = lambda t, v: env('p', t, v)
A = lambda t, v: env('a', t, v)
D = lambda t, v: env('d', t, v)
TL('808', 'Init Thump Live', baseHz=50)
TL('808', 'Long 808', baseHz=45, **P([0.01, 0.05, 0.3], [30, 12, 1, 0]), **A([0.004, 0.4, 1.5], [0.7, 1, 0.6, 0]), **D([0.01, 0.2, 1], [0.3, 0.2, 0.1, 0]), clickLevel=0.2, sub=0.3, toneHz=6000, level=-3)
TL('808', 'Short 808', baseHz=50, **P([0.01, 0.04, 0.2], [28, 10, 1, 0]), **A([0.003, 0.15, 0.5], [0.7, 1, 0.4, 0]), **D([0.01, 0.1, 0.4], [0.3, 0.2, 0.05, 0]), clickLevel=0.25, sub=0.2, level=-3)
TL('808', 'Distorted 808', True, baseHz=48, **P([0.01, 0.05, 0.3], [30, 12, 1, 0]), **A([0.004, 0.4, 1.3], [0.7, 1, 0.6, 0]), **D([0.01, 0.3, 1.2], [0.8, 0.7, 0.5, 0.2]), driveType=1, clickLevel=0.2, sub=0.3, toneHz=4000, level=-6)
TL('808', 'Sub 808', baseHz=40, **P([0.01, 0.04, 0.2], [18, 6, 0, 0]), **A([0.005, 0.5, 1.8], [0.6, 1, 0.7, 0]), **D([0.01, 0.1, 0.5], [0.1, 0.05, 0, 0]), clickLevel=0.1, sub=0.5, toneHz=3000, level=-2)
TL('808', 'Clicky 808', baseHz=52, **P([0.008, 0.04, 0.25], [36, 10, 1, 0]), **A([0.002, 0.3, 1.0], [0.8, 1, 0.5, 0]), **D([0.01, 0.1, 0.5], [0.4, 0.2, 0.05, 0]), clickLevel=0.6, clickTone=4500, clickDecay=0.006, sub=0.2, level=-3)
TL('808', 'Phonk Kick', True, baseHz=46, **P([0.012, 0.06, 0.3], [24, 10, 1, 0]), **A([0.004, 0.35, 1.0], [0.7, 1, 0.5, 0]), **D([0.01, 0.3, 1.0], [0.6, 0.5, 0.3, 0.1]), driveType=2, clickLevel=0.3, sub=0.4, toneHz=3500, level=-6)
# 909 / dance
TL('Dance', 'House Kick', baseHz=55, **P([0.008, 0.04, 0.2], [36, 12, 2, 0]), **A([0.003, 0.12, 0.45], [0.7, 1, 0.35, 0]), **D([0.01, 0.1, 0.4], [0.4, 0.25, 0.1, 0]), clickLevel=0.35, clickTone=3500, sub=0.15, level=-3)
TL('Dance', 'Techno Kick', baseHz=52, **P([0.006, 0.03, 0.15], [40, 14, 2, 0]), **A([0.002, 0.1, 0.4], [0.8, 1, 0.3, 0]), **D([0.01, 0.1, 0.4], [0.7, 0.5, 0.2, 0]), driveType=2, clickLevel=0.4, clickTone=4000, sub=0.1, level=-5)
TL('Dance', 'Hard Techno Kick', True, baseHz=56, **P([0.006, 0.03, 0.15], [44, 16, 3, 0]), **A([0.002, 0.12, 0.35], [0.9, 1, 0.3, 0]), **D([0.01, 0.15, 0.4], [1, 0.9, 0.6, 0.2]), driveType=2, clickLevel=0.5, clickTone=5000, toneHz=8000, level=-8)
TL('Dance', 'Deep House Kick', baseHz=48, **P([0.01, 0.05, 0.25], [24, 8, 1, 0]), **A([0.004, 0.2, 0.6], [0.6, 1, 0.4, 0]), **D([0.01, 0.1, 0.4], [0.2, 0.15, 0.05, 0]), clickLevel=0.25, clickTone=2500, sub=0.3, toneHz=4000, level=-3)
TL('Dance', 'Trance Kick', baseHz=55, **P([0.006, 0.03, 0.12], [42, 14, 2, 0]), **A([0.002, 0.08, 0.3], [0.8, 1, 0.25, 0]), **D([0.01, 0.08, 0.3], [0.5, 0.3, 0.1, 0]), clickLevel=0.4, clickTone=4000, level=-4)
TL('Dance', 'Garage Kick', baseHz=58, **P([0.006, 0.03, 0.12], [30, 10, 1, 0]), **A([0.002, 0.06, 0.22], [0.8, 1, 0.2, 0]), **D([0.01, 0.08, 0.3], [0.3, 0.2, 0.05, 0]), clickLevel=0.35, clickTone=4500, level=-3)
TL('Dance', 'Hardstyle Kick', True, baseHz=50, **P([0.005, 0.04, 0.25], [48, 20, 3, 0]), **A([0.002, 0.15, 0.4], [1, 1, 0.5, 0]), **D([0.005, 0.1, 0.3], [1, 1, 0.8, 0.3]), driveType=3, clickLevel=0.5, clickTone=5000, toneHz=9000, level=-10)
TL('Dance', 'Gabber Kick', True, baseHz=54, **P([0.004, 0.03, 0.2], [40, 20, 4, 0]), **A([0.002, 0.1, 0.3], [1, 1, 0.5, 0]), **D([0.005, 0.1, 0.3], [1, 1, 1, 0.5]), driveType=2, clickLevel=0.5, clickTone=4500, toneHz=10000, level=-10)
TL('Dance', 'Electro Kick', baseHz=52, **P([0.006, 0.03, 0.15], [38, 12, 1, 0]), **A([0.002, 0.1, 0.35], [0.8, 1, 0.3, 0]), **D([0.01, 0.1, 0.4], [0.5, 0.3, 0.1, 0]), driveType=1, clickLevel=0.45, clickTone=3800, level=-4)
TL('Dance', 'Drum & Bass Kick', baseHz=60, **P([0.005, 0.025, 0.1], [36, 12, 2, 0]), **A([0.002, 0.05, 0.18], [0.9, 1, 0.2, 0]), **D([0.01, 0.06, 0.2], [0.4, 0.25, 0.1, 0]), clickLevel=0.45, clickTone=5000, level=-4)
# Hip hop / trap
TL('Trap', 'Trap Kick', baseHz=48, **P([0.008, 0.04, 0.2], [30, 10, 1, 0]), **A([0.003, 0.2, 0.7], [0.7, 1, 0.4, 0]), **D([0.01, 0.1, 0.4], [0.35, 0.2, 0.05, 0]), clickLevel=0.3, clickTone=3000, sub=0.25, level=-3)
TL('Trap', 'Drill Kick', baseHz=52, **P([0.006, 0.03, 0.15], [36, 12, 1, 0]), **A([0.002, 0.1, 0.35], [0.8, 1, 0.3, 0]), **D([0.01, 0.1, 0.4], [0.4, 0.25, 0.1, 0]), clickLevel=0.4, clickTone=3500, level=-3)
TL('Trap', 'Boom Bap Kick', baseHz=56, **P([0.008, 0.04, 0.15], [24, 8, 1, 0]), **A([0.003, 0.08, 0.3], [0.8, 1, 0.3, 0]), **D([0.01, 0.1, 0.4], [0.5, 0.3, 0.1, 0]), driveType=1, clickLevel=0.3, clickTone=2500, toneHz=4500, level=-4)
TL('Trap', 'Lo-Fi Kick', True, baseHz=50, **P([0.01, 0.05, 0.2], [20, 8, 1, 0]), **A([0.004, 0.15, 0.4], [0.7, 1, 0.35, 0]), **D([0.01, 0.1, 0.4], [0.6, 0.4, 0.2, 0]), driveType=1, clickLevel=0.2, clickTone=1800, toneHz=2500, level=-5)
TL('Trap', 'Rage Kick', True, baseHz=50, **P([0.006, 0.03, 0.2], [40, 14, 2, 0]), **A([0.002, 0.15, 0.5], [0.9, 1, 0.4, 0]), **D([0.01, 0.1, 0.4], [0.8, 0.6, 0.3, 0]), driveType=2, clickLevel=0.5, clickTone=4500, level=-7)
TL('Trap', 'Plugg Kick', baseHz=47, **P([0.01, 0.05, 0.25], [22, 8, 1, 0]), **A([0.004, 0.2, 0.5], [0.6, 1, 0.35, 0]), **D([0.01, 0.1, 0.4], [0.2, 0.1, 0, 0]), clickLevel=0.2, clickTone=2200, sub=0.3, toneHz=4000, level=-3)
TL('Trap', 'Memphis Kick', True, baseHz=46, **P([0.01, 0.05, 0.25], [24, 10, 1, 0]), **A([0.004, 0.25, 0.7], [0.7, 1, 0.4, 0]), **D([0.01, 0.2, 0.6], [0.7, 0.6, 0.3, 0]), driveType=1, clickLevel=0.25, clickTone=2000, toneHz=3000, level=-6)
# Acoustic-ish / other
TL('Acoustic', 'Rock Kick', baseHz=60, **P([0.006, 0.03, 0.12], [30, 10, 1, 0]), **A([0.002, 0.06, 0.25], [0.9, 1, 0.25, 0]), **D([0.01, 0.08, 0.3], [0.3, 0.2, 0.05, 0]), clickLevel=0.5, clickTone=4000, clickDecay=0.008, toneHz=7000, level=-4)
TL('Acoustic', 'Jazz Kick', baseHz=64, **P([0.008, 0.04, 0.15], [18, 6, 0, 0]), **A([0.003, 0.08, 0.3], [0.8, 1, 0.25, 0]), **D([0.01, 0.1, 0.4], [0.1, 0.05, 0, 0]), clickLevel=0.3, clickTone=2500, toneHz=4000, level=-4)
TL('Acoustic', 'Big Room Kick', baseHz=52, **P([0.008, 0.05, 0.25], [32, 12, 1, 0]), **A([0.003, 0.2, 0.8], [0.8, 1, 0.5, 0]), **D([0.01, 0.1, 0.5], [0.3, 0.2, 0.1, 0]), clickLevel=0.4, clickTone=3500, sub=0.2, toneHz=6000, level=-4)
TL('Acoustic', 'Tight Pop Kick', baseHz=58, **P([0.006, 0.03, 0.12], [34, 10, 1, 0]), **A([0.002, 0.06, 0.2], [0.9, 1, 0.2, 0]), **D([0.01, 0.08, 0.3], [0.35, 0.2, 0.05, 0]), clickLevel=0.45, clickTone=4200, level=-4)
TL('Acoustic', 'Taiko Thump', True, baseHz=44, **P([0.01, 0.08, 0.4], [14, 6, 1, 0]), **A([0.005, 0.4, 1.5], [0.7, 1, 0.6, 0]), **D([0.01, 0.1, 0.5], [0.1, 0.05, 0, 0]), clickLevel=0.3, clickTone=1500, clickDecay=0.012, toneHz=5000, level=-3)
TL('FX', 'Laser Kick', True, baseHz=60, **P([0.02, 0.1, 0.3], [72, 36, 6, 0]), **A([0.002, 0.15, 0.4], [0.9, 1, 0.3, 0]), **D([0.01, 0.1, 0.4], [0.3, 0.2, 0.1, 0]), clickLevel=0.1, level=-4)
TL('FX', 'Reverse Swell', True, baseHz=50, **P([0.2, 0.5, 0.8], [0, 6, 18, 30]), **A([0.3, 0.7, 0.9], [0, 0.4, 1, 0]), **D([0.3, 0.6, 0.9], [0, 0.2, 0.6, 0]), clickLevel=0, sub=0.3, level=-5)
TL('FX', 'Double Hit', True, baseHz=52, **P([0.01, 0.12, 0.3], [30, 2, 24, 0]), **A([0.004, 0.1, 0.14], [0.8, 0.2, 1, 0]), **D([0.01, 0.1, 0.4], [0.4, 0.2, 0.4, 0]), clickLevel=0.4, level=-4)
TL('FX', 'Tom Thump', True, baseHz=90, **P([0.01, 0.1, 0.5], [12, 6, 2, 0]), **A([0.004, 0.3, 0.9], [0.8, 1, 0.5, 0]), **D([0.01, 0.1, 0.4], [0.1, 0.05, 0, 0]), clickLevel=0.2, clickTone=2000, toneHz=5000, level=-4)
# Patterns
TL('Pattern', 'Four On The Floor', baseHz=55, **P([0.008, 0.04, 0.2], [36, 12, 2, 0]), **A([0.003, 0.12, 0.45], [0.7, 1, 0.35, 0]), clickLevel=0.35, **pattern([1, 0, 0, 0]), level=-3)
TL('Pattern', 'Trap Pattern', baseHz=48, **P([0.008, 0.04, 0.2], [30, 10, 1, 0]), **A([0.003, 0.2, 0.7], [0.7, 1, 0.4, 0]), clickLevel=0.3, sub=0.25, **pattern([1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0]), level=-3)
TL('Pattern', 'Drill Pattern', baseHz=52, **P([0.006, 0.03, 0.15], [36, 12, 1, 0]), **A([0.002, 0.1, 0.35], [0.8, 1, 0.3, 0]), clickLevel=0.4, **pattern([1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0]), level=-3)
TL('Pattern', 'Boom Bap Pattern', baseHz=56, **P([0.008, 0.04, 0.15], [24, 8, 1, 0]), **A([0.003, 0.08, 0.3], [0.8, 1, 0.3, 0]), driveType=1, clickLevel=0.3, clickTone=2500, **pattern([1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0]), level=-4)
TL('Pattern', 'Reggaeton Pattern', baseHz=52, **P([0.008, 0.04, 0.2], [30, 10, 1, 0]), **A([0.003, 0.12, 0.4], [0.8, 1, 0.3, 0]), clickLevel=0.35, **pattern([1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0]), level=-3)
TL('Pattern', 'Broken Beat', True, baseHz=55, **P([0.006, 0.03, 0.12], [34, 10, 1, 0]), **A([0.002, 0.08, 0.3], [0.9, 1, 0.25, 0]), clickLevel=0.4, **pattern([1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0]), level=-4)
TL('Pattern', 'Eighth Pump', baseHz=50, **P([0.008, 0.04, 0.2], [30, 10, 1, 0]), **A([0.003, 0.1, 0.3], [0.8, 1, 0.3, 0]), clickLevel=0.3, **pattern([1, 0], rate=1), level=-3)
TL('Pattern', 'Rolling 32nds', True, baseHz=58, **P([0.005, 0.025, 0.1], [36, 12, 2, 0]), **A([0.002, 0.04, 0.12], [0.9, 1, 0.2, 0]), clickLevel=0.45, **pattern([1, 1, 1, 1, 1, 0, 1, 0] * 2, rate=2), level=-5)

# ------------------------------------------------------------------------------------------------ Pollen packs (the Exo engine behind macro pages)
# Each pack is a category; macros 1-4 are wired through the mod matrix so the pack page shows named macro knobs.
PO = lambda pack, name, adv=False, props=None, mods=None, **k: preset('Pollen', pack, name, adv, props, mods, **k)
def variants(pack, base_name, base, mods=None, props=None, vars=None):
    """base: dict of engine params (may hold 'props'); vars: list of (suffix, overrides) - emits base + each variant as its own preset."""
    def emit(name, d):
        d = dict(d); pr = dict(props or {}); pr.update(d.pop('props', {}) or {})
        PO(pack, name, False, pr, mods, **d)
    emit(base_name, base)
    for suffix, ov in (vars or []):
        d = dict(base); d.update(ov); emit(f'{base_name} {suffix}', d)
MAC = lambda m1, m2, m3, m4: [(M1, m1, 0.6), (M2, m2, 0.5), (M3, m3, 0.5), (M4, m4, 0.5)]
BRIGHT = ('Bright', dict(f1Cut=6000, f1Env=0.3))
SOFT = ('Soft', dict(f1Cut=1200, aA=0.08, f1Env=0.1))
WIDE = ('Wide', dict(uniDetune=14, uniSpread=1, width=1.6, fxChorus=0.35))
DARK = ('Dark', dict(f1Cut=700, f1Res=0.2))
# Keys
variants('Keys', 'Grand Piano-ish', dict(o1Wave=W_TABLE, o1Morph=T_BRIGHT - 0.05, o2Wave=W_TRI, o2Semi=12, o2Level=0.25, f1Cut=4200, f1Env=0.5, fD=0.9, fS=0, aD=3.0, aS=0.05, aR=0.5, ampVel=1, fxReverb=0.2, level=-7), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[BRIGHT, SOFT, ('Felt', dict(f1Cut=1500, aD=2.0, fxReverb=0.3))])
variants('Keys', 'Electric Piano', dict(o1Wave=W_SINE, o2Wave=W_SINE, o2Semi=24, o2Level=0.2, fmAmt=0.22, f1Cut=5000, aD=1.8, aS=0.3, aR=0.4, ampVel=1, fD=0.4, fS=0, fxChorus=0.15, fxReverb=0.15, level=-6), [(VEL, 'fm', 0.5), (ENV2, 'fm', 0.3)] + MAC('fm', 'cutoff', 'amp', 'pan'), vars=[('Tremolo', dict(lfo2Rate=4.5, lfo2Amp=0.4)), ('Phased', dict(fxChorus=0.6)), ('Bell', dict(fmAmt=0.5, o2Semi=19))])
variants('Keys', 'Clavinet', dict(o1Wave=W_PULSE, o1Pw=0.12, o2Wave=W_SAW, o2Level=0.3, f1Cut=1800, f1Res=0.3, f1Env=0.6, fD=0.12, fS=0.1, aD=0.6, aS=0.2, aR=0.08, ampVel=0.9, level=-7), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[('Wah', dict(f1Res=0.6, lfo1Rate=2.0, lfo1Cut=0.4)), ('Muted', dict(f1Cut=900, aD=0.3))])
variants('Keys', 'Organ', dict(o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_TABLE, o2Morph=T_ORGAN, o2Semi=12, o2Level=0.5, f1Cut=20000, f1Env=0, aA=0.003, aS=1, aR=0.04, fxChorus=0.35, level=-7), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Rotary', dict(fxChorus=0.7, lfo2Rate=6, lfo2Amp=0.3)), ('Percussive', dict(aD=0.3, aS=0.7))])
variants('Keys', 'Soft Keys', dict(o1Wave=W_TRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=3200, f1Env=0.4, fD=0.6, fS=0.2, aD=1.2, aS=0.5, aR=0.5, ampVel=0.9, fxReverb=0.2, level=-7), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[WIDE, DARK])
variants('Keys', 'Harpsichord', dict(o1Wave=W_PULSE, o1Pw=0.08, o2Wave=W_PULSE, o2Pw=0.1, o2Semi=12, o2Level=0.4, f1Cut=4000, f1Env=0.4, aD=1.0, aS=0, aR=0.15, fD=0.3, fS=0, level=-8), MAC('cutoff', 'pw', 'amp', 'pan'), vars=[('Lute', dict(f1Cut=2200, aD=0.8))])
variants('Keys', 'Toy Piano', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=6000, f1Env=0.3, aD=1.2, aS=0, aR=0.4, fD=0.3, fS=0, fxReverb=0.3, level=-7), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Music Box', dict(aD=2.0, o2Semi=24))])
variants('Keys', 'Vocal Keys', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_TRI, o2Level=0.3, f1Cut=3000, f1Env=0.4, aD=1.2, aS=0.4, aR=0.4, fD=0.5, fS=0.2, fxReverb=0.2, level=-7), [(ENV2, 'morph', 0.2)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[WIDE])
# Pads
variants('Pads', 'Warm Analog Pad', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=14, subLevel=0.2, f1Type=LADDER, f1Cut=1400, f1Res=0.12, aA=0.4, aS=0.9, aR=1.2, drift=0.5, fxChorus=0.3, props={'unison': 2}, level=-9), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[BRIGHT, DARK, WIDE])
variants('Pads', 'Morphing Pad', dict(o1Wave=W_TABLE, o1Morph=0.35, o2Wave=W_TABLE, o2Morph=0.6, o2Fine=7, f1Cut=2500, f1Env=0.2, aA=0.8, aS=1, aR=2.0, fA=1.5, fS=0.6, lfo1Rate=0.08, fxChorus=0.4, fxReverb=0.45, fxSize=0.85, width=1.5, level=-10), [(LFO1, 'morph', 0.5), (LFO2, 'pan', 0.4)] + MAC('morph', 'cutoff', 'amp', 'resonance'), vars=[('Slow', dict(aA=2.0, aR=3.5)), ('Glassy', dict(o1Morph=T_GLASS, o2Morph=T_GLASS))])
variants('Pads', 'Strings', dict(o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=10, o1Fine=-10, f1Cut=1800, f1Env=0.25, aA=0.5, aS=1, aR=1.6, drift=0.4, uniDetune=14, fxChorus=0.5, fxReverb=0.35, props={'unison': 4}, level=-11), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Swell', dict(aA=1.5)), ('Staccato', dict(aA=0.02, aR=0.3)), DARK])
variants('Pads', 'Choir', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_TABLE, o2Morph=T_VOCAL + 0.05, o2Fine=7, f1Type=BP, f1Cut=1200, f1Res=0.3, aA=0.9, aS=1, aR=2.0, lfo2Rate=0.25, fxChorus=0.45, fxReverb=0.5, fxSize=0.85, level=-10), [(LFO2, 'morph', 0.2)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Ahh', dict(o1Morph=T_VOCAL + 0.08)), ('Ooh', dict(o1Morph=T_VOCAL - 0.06, f1Cut=900))])
variants('Pads', 'Glass Pad', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_GLASS, o2Semi=12, o2Level=0.4, f1Cut=4000, aA=1.2, aS=1, aR=2.5, lfo1Rate=0.1, fxReverb=0.55, fxSize=0.9, fxDelay=0.3, fxDelayTime=1.5, fxDelayFb=0.5, level=-11), [(LFO1, 'morph', 0.1)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Shimmer', dict(o2Semi=19, fxReverb=0.7))])
variants('Pads', 'Dark Drone', dict(o1Wave=W_TABLE, o1Morph=T_FOLD, o2Wave=W_TABLE, o2Morph=T_FOLD, o2Semi=-12, o2Fine=3, f1Type=LP24, f1Cut=900, f1Res=0.3, aA=2.5, aS=1, aR=4, lfo1Rate=0.05, lfo2Rate=0.07, fxReverb=0.7, fxSize=0.95, level=-10), [(LFO1, 'morph', 0.7), (LFO2, 'cutoff', 0.4)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[('Evolving', dict(lfo1Rate=0.02, fA=4, fS=1, f1Env=0.5))])
variants('Pads', 'Vox Pad', dict(o1Wave=W_PULSE, o1Pw=0.25, o2Wave=W_SAW, o2Fine=5, f1Type=1, f1Cut=1100, f1Res=0.4, aA=0.6, aS=1, aR=1.5, lfo1Rate=0.25, lfo1Cut=0.2, fxChorus=0.6, fxReverb=0.4, level=-10), MAC('cutoff', 'pw', 'amp', 'pan'), vars=[WIDE])
# Plucks
variants('Plucks', 'Tight Pluck', dict(o1Wave=W_SAW, o2Wave=W_SQR, o2Semi=12, o2Level=0.35, f1Cut=500, f1Res=0.25, f1Env=0.9, fD=0.16, fS=0, aD=0.35, aS=0, aR=0.2, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[('Long', dict(aD=0.9, fD=0.4)), ('Dry', dict(fxDelay=0)), WIDE])
variants('Plucks', 'Table Pluck', dict(o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_SQR, o2Semi=12, o2Level=0.35, f1Cut=500, f1Res=0.25, f1Env=0.9, fD=0.16, fS=0, aD=0.35, aS=0, aR=0.2, fxDelay=0.25, fxDelayTime=0.75, fxDelayFb=0.35, level=-6), [(ENV2, 'morph', -0.4)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Glass', dict(o1Morph=T_GLASS)), ('Vocal', dict(o1Morph=T_VOCAL))])
variants('Plucks', 'Marimba', dict(o1Wave=W_SINE, o2Wave=W_TRI, o2Semi=24, o2Level=0.3, f1Cut=3000, f1Env=0.6, fD=0.08, fS=0, aD=0.5, aS=0, aR=0.3, ampVel=1, level=-5), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Kalimba', dict(o1Wave=W_TRI, o2Semi=12, aD=0.7, fxReverb=0.25)), ('Vibes', dict(aD=2.5, lfo2Rate=5.5, lfo2Amp=0.25, fxReverb=0.3))])
variants('Plucks', 'Harp', dict(o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=3500, f1Env=0.5, fD=0.3, fS=0, aD=1.8, aS=0, aR=0.8, ampVel=0.7, fxReverb=0.35, fxSize=0.7, level=-6), MAC('cutoff', 'amp', 'pan', 'morph'), vars=[('Nylon', dict(f1Cut=2200, aD=1.2))])
variants('Plucks', 'Future Pluck', dict(o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=15, o1Fine=-15, uniDetune=20, f1Cut=700, f1Res=0.2, f1Env=1, fD=0.25, fS=0, aD=0.5, aS=0, aR=0.35, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.45, fxReverb=0.3, width=1.5, props={'unison': 4}, level=-8), [(ENV2, 'morph', 0.5)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[('Big Room', dict(fxReverb=0.5, uniDetune=28)), ('Short', dict(aD=0.25, fD=0.12))])
variants('Plucks', 'Organ Stab', dict(o1Wave=W_TABLE, o1Morph=T_ORGAN, o2Wave=W_TABLE, o2Morph=T_ORGAN, o2Semi=12, o2Level=0.4, f1Cut=4000, f1Env=0.3, aD=0.3, aS=0, aR=0.2, fD=0.1, fS=0, fxReverb=0.25, level=-6), MAC('cutoff', 'morph', 'amp', 'pan'), vars=[('Rave', dict(o1Wave=W_SAW, o2Wave=W_SQR, f1Res=0.3, uniDetune=16, props={'unison': 3}))])
# Bass
variants('Bass', 'Sub', dict(o1Wave=W_SINE, o1Level=1, o2Level=0, subLevel=0.6, f1Cut=400, f1Env=0.1, aD=0.5, aS=0.9, aR=0.15, glide=0.06, props={'polyMode': MONO, 'glideMode': 2}, level=-3), MAC('cutoff', 'amp', 'pitch', 'resonance'), vars=[('808', dict(aD=1.4, aS=0.3, drive=0.45, driveType=1, glide=0.12)), ('Punch', dict(f1Env=0.5, fD=0.1, fS=0))])
variants('Bass', 'Reese', dict(o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=18, o1Fine=-18, subLevel=0.35, f1Cut=900, f1Res=0.25, uniDetune=20, uniSpread=1, aS=1, aR=0.2, width=1.4, props={'unison': 4, 'polyMode': MONO}, level=-8), MAC('cutoff', 'resonance', 'amp', 'morph'), vars=[DARK, ('Wide', dict(uniDetune=30, width=1.7))])
variants('Bass', 'Acid Bass', dict(o1Wave=W_SAW, o2Level=0, f1Type=LADDER, f1Cut=380, f1Res=0.72, f1Env=0.75, fD=0.22, fS=0, aD=0.3, aS=0.6, aR=0.1, drive=0.35, driveType=1, glide=0.09, props={'polyMode': MONO, 'glideMode': 2}, level=-6), [(MODX, 'cutoff', 0.6)] + MAC('cutoff', 'resonance', 'amp', 'pitch'), vars=[('Square', dict(o1Wave=W_SQR)), ('Distorted', dict(drive=0.7, driveType=2))])
variants('Bass', 'Pluck Bass', dict(o1Wave=W_TABLE, o1Morph=T_BRIGHT, o2Wave=W_TRI, o2Semi=-12, subLevel=0.4, f1Cut=320, f1Res=0.3, f1Env=0.85, fD=0.28, fS=0, aD=0.45, aS=0.2, aR=0.12, ampVel=0.8, level=-5), MAC('cutoff', 'resonance', 'amp', 'morph'), vars=[('Soft', dict(f1Env=0.5, aD=0.6)), ('Clicky', dict(fD=0.08, f1Env=1))])
variants('Bass', 'Wobble', dict(o1Wave=W_TABLE, o1Morph=T_DIGITAL, o2Wave=W_SQR, o2Semi=-12, subLevel=0.3, f1Cut=300, f1Res=0.45, lfo1Shape=1, lfo1Sync=1, lfo1Rate=6, lfo1Cut=0.9, drive=0.5, driveType=2, aS=1, aR=0.1, props={'polyMode': MONO}, level=-8), [(LFO1, 'morph', 0.5)] + MAC('cutoff', 'resonance', 'amp', 'morph'), vars=[('Slow', dict(lfo1Rate=2)), ('Fast', dict(lfo1Rate=12))])
variants('Bass', 'Growl', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SQR, o2Semi=-12, f1Type=BP, f1Cut=700, f1Res=0.5, routing=1, f2Type=HP12, f2Cut=60, lfo1Shape=0, lfo1Rate=0.8, lfo1Sync=1, drive=0.5, driveType=3, aS=1, aR=0.15, props={'polyMode': MONO}, level=-8), [(LFO1, 'morph', 0.7)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[('Talking', dict(lfo1Rate=3, f1Res=0.7))])
variants('Bass', 'Ladder Stab', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=-6, f1Type=LADDER, f1Cut=250, f1Res=0.4, f1Env=0.7, fD=0.35, fS=0.05, aD=0.5, aS=0.5, aR=0.1, drive=0.2, props={'polyMode': MONO}, level=-5), MAC('cutoff', 'resonance', 'amp', 'pitch'), vars=[('Round', dict(f1Cut=180, drive=0))])
# Brass
variants('Brass', 'Synth Brass', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=8, f1Cut=800, f1Res=0.15, f1Env=0.8, fA=0.04, fD=0.3, fS=0.5, aA=0.02, aD=0.4, aS=0.8, aR=0.15, ampVel=0.8, props={'unison': 2}, level=-7), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[('Stab', dict(aS=0.3, aD=0.3)), ('Soft', dict(aA=0.15, f1Env=0.4)), WIDE])
variants('Brass', 'Horn Section', dict(o1Wave=W_SAW, o2Wave=W_PULSE, o2Pw=0.4, o2Fine=-6, f1Cut=1100, f1Env=0.6, aA=0.08, aS=0.9, aR=0.2, fA=0.12, fS=0.4, drift=0.5, fxChorus=0.4, level=-8), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Trumpets', dict(f1Cut=2200, o2Pw=0.3)), ('Trombones', dict(f1Cut=700, o1Semi=-12, o2Semi=-12))])
variants('Brass', 'Trap Brass', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=12, o1Fine=-12, f1Cut=2500, f1Env=0.5, fA=0.02, fD=0.3, fS=0.6, aA=0.01, aS=0.9, aR=0.25, drive=0.2, driveType=1, uniDetune=16, fxReverb=0.3, props={'unison': 3}, level=-9), MAC('cutoff', 'amp', 'resonance', 'pan'), vars=[('Dark', dict(f1Cut=1200)), ('Octave', dict(o2Semi=-12))])
variants('Brass', 'French Horns', dict(o1Wave=W_TRI, o2Wave=W_SAW, o2Fine=5, o2Level=0.4, f1Cut=1300, f1Env=0.4, aA=0.15, aS=1, aR=0.5, fA=0.3, fS=0.5, lfo1Rate=4.5, fxReverb=0.35, level=-8), [(LFO1, 'pitch', 0.006)] + MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Distant', dict(f1Cut=800, fxReverb=0.6))])
variants('Brass', 'Hoover', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Semi=-12, o1Fine=25, o2Fine=-25, uniDetune=28, f1Cut=1500, f1Env=0.6, fA=0.35, fD=0.8, fS=0.4, aA=0.05, aS=1, aR=0.4, fxChorus=0.3, fxReverb=0.25, props={'unison': 4}, level=-9), [(ENV2, 'pitch', -0.15), (ENV2, 'pw', 0.3)] + MAC('cutoff', 'amp', 'pitch', 'resonance'), vars=[])
# Strings
variants('Strings', 'Ensemble Strings', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=9, f1Cut=1400, f1Res=0.1, f1Env=0.2, aA=0.5, aS=1, aR=1.4, fA=0.6, fS=0.6, drift=0.5, fxChorus=0.6, fxReverb=0.3, props={'unison': 2}, level=-9), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Marcato', dict(aA=0.03, aR=0.4)), ('Legato', dict(aA=0.9, props={'unison': 2, 'polyMode': LEGATO}, glide=0.08)), DARK])
variants('Strings', 'Cello Section', dict(o1Wave=W_SAW, o2Wave=W_PULSE, o2Pw=0.35, o2Fine=6, o1Semi=-12, o2Semi=-12, f1Cut=900, f1Env=0.3, aA=0.3, aS=1, aR=0.8, drift=0.4, lfo1Rate=5, fxReverb=0.3, level=-8), [(LFO1, 'pitch', 0.005)] + MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Solo', dict(props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.1, lfo1Rate=5.5))])
variants('Strings', 'Violins', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=7, o2Semi=12, o2Level=0.3, f1Cut=2600, f1Env=0.3, aA=0.25, aS=1, aR=0.7, drift=0.3, lfo1Rate=5.5, fxReverb=0.35, level=-9), [(LFO1, 'pitch', 0.006)] + MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Tremolo', dict(lfo2Rate=8, lfo2Amp=0.6)), ('Pizzicato', dict(aA=0.003, aD=0.3, aS=0, aR=0.2, f1Env=0.6, fD=0.1, fS=0))])
variants('Strings', 'Cinematic Strings', dict(o1Wave=W_TABLE, o1Morph=T_SAWTRI, o2Wave=W_SAW, o2Fine=12, o1Fine=-12, f1Cut=1600, f1Env=0.3, aA=1.2, aS=1, aR=2.5, fA=2, fS=0.6, uniDetune=14, fxChorus=0.4, fxReverb=0.55, fxSize=0.9, props={'unison': 4}, width=1.6, level=-11), MAC('cutoff', 'amp', 'morph', 'resonance'), vars=[('Low', dict(o1Semi=-12, o2Semi=-12, f1Cut=800)), ('Rising', dict(fA=4, f1Env=0.6))])
variants('Strings', 'Synth Strings 80s', dict(o1Wave=W_PULSE, o1Pw=0.4, o2Wave=W_SAW, o2Fine=10, f1Cut=2000, f1Env=0.2, aA=0.4, aS=1, aR=1.2, lfo2Rate=0.4, lfo2Pw=0.4, fxChorus=0.7, fxReverb=0.3, level=-9), MAC('cutoff', 'pw', 'amp', 'pan'), vars=[])
# Choir
variants('Choir', 'Ahh Choir', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL + 0.06, o2Wave=W_TABLE, o2Morph=T_VOCAL, o2Fine=7, f1Type=BP, f1Cut=1300, f1Res=0.3, aA=0.8, aS=1, aR=1.8, lfo2Rate=0.3, fxChorus=0.45, fxReverb=0.5, fxSize=0.85, level=-10), [(LFO2, 'morph', 0.15)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Mens', dict(o1Semi=-12, o2Semi=-12, f1Cut=800)), ('Womens', dict(o1Semi=12, o2Semi=12, f1Cut=2200)), WIDE])
variants('Choir', 'Ooh Choir', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL - 0.06, o2Wave=W_TABLE, o2Morph=T_VOCAL - 0.08, o2Fine=5, f1Type=BP, f1Cut=800, f1Res=0.35, aA=1.0, aS=1, aR=2.0, fxChorus=0.4, fxReverb=0.55, fxSize=0.9, level=-10), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Hum', dict(f1Cut=500, f1Res=0.5))])
variants('Choir', 'Gospel Choir', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL + 0.04, o2Wave=W_SAW, o2Level=0.35, o2Fine=-8, f1Type=BP, f1Cut=1500, f1Res=0.25, aA=0.15, aS=1, aR=1.0, uniDetune=12, fxChorus=0.5, fxReverb=0.45, props={'unison': 3}, level=-10), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Stacked', dict(props={'unison': 6}, uniDetune=18, width=1.6))])
variants('Choir', 'Synth Vox', dict(o1Wave=W_PULSE, o1Pw=0.18, o2Wave=W_PULSE, o2Pw=0.82, o2Fine=6, f1Type=1, f1Cut=1500, f1Res=0.25, f1Env=0.1, aA=0.7, aS=1, aR=1.8, lfo2Rate=0.3, lfo2Pw=0.5, fxChorus=0.65, fxReverb=0.45, level=-10), MAC('cutoff', 'pw', 'amp', 'pan'), vars=[('Robot', dict(f1Res=0.6, fxChorus=0.2))])
variants('Choir', 'Whisper Pad', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_NOISE, o2Level=0.25, f1Type=BP, f1Cut=2200, f1Res=0.4, aA=1.5, aS=1, aR=2.5, lfo1Rate=0.2, lfo1Cut=0.3, fxReverb=0.6, fxSize=0.9, level=-11), MAC('cutoff', 'morph', 'amp', 'pan'), vars=[])
# Bells
variants('Bells', 'Glass Bell', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=19, o2Fine=4, o2Level=0.45, fmAmt=0.12, f1Cut=8000, aD=2.5, aS=0, aR=1.5, fxReverb=0.35, fxSize=0.7, level=-7), [(KEY, 'amp', -0.3)] + MAC('morph', 'fm', 'amp', 'pan'), vars=[('Long', dict(aD=5, aR=3)), ('Short', dict(aD=0.8, aR=0.5))])
variants('Bells', 'Tubular', dict(o1Wave=W_SINE, o2Wave=W_SINE, o2Semi=24, o2Fine=-6, o2Level=0.5, fmAmt=0.3, f1Cut=9000, aD=5, aS=0, aR=3, fxReverb=0.4, fxSize=0.8, level=-8), MAC('fm', 'cutoff', 'amp', 'pan'), vars=[('Church', dict(fxReverb=0.7, fxSize=0.95))])
variants('Bells', 'Music Box', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=6000, f1Env=0.3, aD=1.5, aS=0, aR=0.8, fD=0.3, fS=0, fxReverb=0.3, level=-7), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Celesta', dict(aD=2.0, o2Semi=24))])
variants('Bells', 'Gamelan', dict(o1Wave=W_SINE, o2Wave=W_SINE, o2Semi=15, o2Fine=20, o2Level=0.5, fmAmt=0.25, f1Cut=7000, aD=2.0, aS=0, aR=1.2, fxReverb=0.3, level=-8), MAC('fm', 'cutoff', 'amp', 'pan'), vars=[('Metallic', dict(fmAmt=0.5, o2Semi=18))])
variants('Bells', 'Chime Pad', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_SINE, o2Semi=19, o2Level=0.4, f1Cut=6000, aA=0.5, aS=1, aR=3.0, lfo1Rate=0.06, fxReverb=0.7, fxSize=0.95, fxDelay=0.4, fxDelayTime=1.5, fxDelayFb=0.6, level=-11), [(LFO1, 'morph', 0.4)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[])
# Textures
variants('Textures', 'Shimmer Wash', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_TABLE, o2Morph=T_SINE, o2Semi=19, o2Level=0.4, f1Cut=6000, aA=2.0, aS=1, aR=4.0, lfo1Rate=0.06, fxReverb=0.75, fxSize=0.95, fxDelay=0.4, fxDelayTime=1.5, fxDelayFb=0.6, level=-11), [(LFO1, 'morph', 0.5)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Darker', dict(f1Cut=1500))])
variants('Textures', 'Digital Rain', dict(o1Wave=W_TABLE, o1Morph=T_DIGITAL, o2Level=0, f1Type=BP, f1Cut=2000, f1Res=0.5, aD=0.15, aS=0, aR=0.1, lfo1Shape=4, lfo1Rate=8, fxDelay=0.4, fxDelayTime=0.375, fxDelayFb=0.6, fxReverb=0.4, level=-8), [(LFO1, 'morph', 0.8), (LFO1, 'pitch', 0.3)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[('Slow', dict(lfo1Rate=2, aD=0.6))])
variants('Textures', 'Fold Drone', dict(o1Wave=W_TABLE, o1Morph=T_FOLD, o2Wave=W_TABLE, o2Morph=T_FOLD, o2Semi=-12, o2Fine=3, f1Type=LP24, f1Cut=900, f1Res=0.3, aA=2.5, aS=1, aR=4, lfo1Rate=0.05, lfo2Rate=0.07, fxReverb=0.7, fxSize=0.95, level=-10), [(LFO1, 'morph', 0.7), (LFO2, 'cutoff', 0.4)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[])
variants('Textures', 'Wind', dict(o1Wave=W_NOISE, o2Level=0, f1Type=BP, f1Cut=1500, f1Res=0.4, lfo1Shape=5, lfo1Rate=0.2, lfo1Cut=0.9, aA=2.0, aS=1, aR=3.0, fxReverb=0.5, fxSize=0.9, level=-9), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[('Storm', dict(f1Res=0.6, lfo1Rate=0.6))])
variants('Textures', 'Riser', dict(o1Wave=W_TABLE, o1Morph=0, o2Wave=W_NOISE, o2Level=0.4, f1Type=BP, f1Cut=300, f1Res=0.5, f1Env=1, fA=4.0, fS=1, aA=3.5, aS=1, aR=0.5, uniDetune=30, fxReverb=0.5, props={'unison': 4}, level=-10), [(ENV2, 'morph', 1.0), (ENV2, 'pitch', 0.4)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[('Fast', dict(fA=1.5, aA=1.2)), ('Downlifter', dict(f1Env=-1, fA=0.001, fD=4, fS=0))])
variants('Textures', 'Alien Talk', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SINE, o2Semi=5, ring=0.6, f1Type=BP, f1Cut=900, f1Res=0.6, lfo1Shape=4, lfo1Rate=7, lfo1Cut=0.8, aS=1, aR=0.2, level=-8), [(LFO1, 'morph', 0.9)] + MAC('cutoff', 'morph', 'amp', 'resonance'), vars=[])
# Trap Melodies
variants('Trap Melodies', 'Dark Bell', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=3500, f1Env=0.3, aD=2.0, aS=0, aR=0.8, fD=0.4, fS=0, fxReverb=0.45, fxSize=0.8, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, level=-8), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Detuned', dict(uniDetune=12, props={'unison': 2})), ('Short', dict(aD=0.8))])
variants('Trap Melodies', 'Trap Flute', dict(o1Wave=W_TRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.2, f1Cut=3000, f1Env=0.2, aA=0.03, aS=1, aR=0.4, lfo1Rate=5.5, fxReverb=0.4, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.06, level=-7), [(LFO1, 'pitch', 0.012)] + MAC('cutoff', 'amp', 'pitch', 'pan'), vars=[('Breathy', dict(o2Wave=W_NOISE, o2Level=0.15, f1Cut=2200))])
variants('Trap Melodies', 'Plucked Guitar', dict(o1Wave=W_SAW, o2Wave=W_PULSE, o2Pw=0.2, o2Level=0.4, f1Type=LP12, f1Cut=1500, f1Env=0.5, fD=0.3, fS=0.1, aD=1.0, aS=0.1, aR=0.3, ampVel=0.9, fxReverb=0.3, fxDelay=0.25, fxDelayTime=0.75, level=-7), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Muted', dict(f1Cut=800, aD=0.4)), ('Spanish', dict(f1Cut=2500, aD=1.5))])
variants('Trap Melodies', 'Eerie Keys', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=2400, f1Env=0.3, aD=1.5, aS=0.2, aR=0.7, fD=0.5, fS=0, lfo1Rate=4, fxReverb=0.5, fxSize=0.9, level=-8), [(LFO1, 'pitch', 0.008)] + MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Detuned', dict(o2Fine=14))])
variants('Trap Melodies', 'Choir Stab', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL + 0.05, o2Wave=W_TABLE, o2Morph=T_VOCAL, o2Fine=7, f1Type=BP, f1Cut=1400, f1Res=0.3, aA=0.01, aD=0.6, aS=0.3, aR=0.6, fxReverb=0.5, level=-9), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[])
variants('Trap Melodies', 'Lead Synth', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=10, f1Cut=3000, f1Env=0.3, aS=1, aR=0.3, lfo1Rate=5.5, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, fxReverb=0.3, props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.06, level=-7), [(WHEEL, 'pitch', 0.03), (LFO1, 'pitch', 0.01)] + MAC('cutoff', 'amp', 'pitch', 'resonance'), vars=[('Square', dict(o1Wave=W_SQR, o2Wave=W_PULSE, o2Pw=0.3))])
# Drill Melodies
variants('Drill Melodies', 'Drill Piano', dict(o1Wave=W_TABLE, o1Morph=T_BRIGHT - 0.05, o2Wave=W_TRI, o2Semi=12, o2Level=0.25, f1Cut=3000, f1Env=0.5, fD=0.9, fS=0, aD=2.0, aS=0.05, aR=0.5, ampVel=1, fxReverb=0.4, fxSize=0.8, level=-7), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Dark', dict(f1Cut=1400)), ('Detuned', dict(o2Fine=12, uniDetune=10, props={'unison': 2}))])
variants('Drill Melodies', 'Drill Bell', dict(o1Wave=W_TABLE, o1Morph=T_GLASS, o2Wave=W_SINE, o2Semi=12, o2Level=0.3, f1Cut=3500, f1Env=0.3, aD=1.5, aS=0, aR=0.8, fD=0.4, fS=0, fxReverb=0.45, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, level=-8), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Slide', dict(props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.1))])
variants('Drill Melodies', 'Drill Strings', dict(o1Wave=W_SAW, o2Wave=W_SAW, o2Fine=9, f1Cut=1200, f1Env=0.2, aA=0.3, aS=1, aR=1.0, drift=0.4, fxChorus=0.5, fxReverb=0.4, props={'unison': 2}, level=-9), MAC('cutoff', 'amp', 'pan', 'resonance'), vars=[('Staccato', dict(aA=0.01, aR=0.3)), ('Low', dict(o1Semi=-12, o2Semi=-12))])
variants('Drill Melodies', 'Drill Flute', dict(o1Wave=W_TRI, o2Wave=W_SINE, o2Semi=12, o2Level=0.2, f1Cut=2800, f1Env=0.2, aA=0.03, aS=1, aR=0.4, lfo1Rate=5.5, fxReverb=0.45, props={'polyMode': LEGATO, 'glideMode': 2}, glide=0.08, level=-7), [(LFO1, 'pitch', 0.012)] + MAC('cutoff', 'amp', 'pitch', 'pan'), vars=[])
variants('Drill Melodies', 'Drill Vox', dict(o1Wave=W_TABLE, o1Morph=T_VOCAL, o2Wave=W_TABLE, o2Morph=T_VOCAL + 0.06, o2Fine=6, f1Type=BP, f1Cut=1200, f1Res=0.3, aA=0.3, aS=1, aR=1.2, fxChorus=0.4, fxReverb=0.5, level=-10), MAC('morph', 'cutoff', 'amp', 'pan'), vars=[('Chopped', dict(aA=0.005, aD=0.25, aS=0, aR=0.2))])
variants('Drill Melodies', 'Drill Pluck', dict(o1Wave=W_SAW, o2Wave=W_SQR, o2Semi=12, o2Level=0.35, f1Cut=600, f1Res=0.25, f1Env=0.9, fD=0.2, fS=0, aD=0.4, aS=0, aR=0.3, fxDelay=0.3, fxDelayTime=0.75, fxDelayFb=0.4, fxReverb=0.3, level=-6), MAC('cutoff', 'resonance', 'amp', 'pan'), vars=[('Dark', dict(f1Cut=350))])

# ------------------------------------------------------------------------------------------------ measured level trims
_lv = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'preset_levels_181.json')
if os.path.exists(_lv):
    trims = json.load(open(_lv))
    for bank, items in banks.items():
        for n, (cat, name, adv, vals, props, mods) in enumerate(items):
            key = f'{bank}|{cat} | {name}'
            if key in trims:
                lvl = 'masterLevel' if bank in ('Carapace', 'Live Drum Synth') else 'level'
                default = -3
                vals[lvl] = round(max(-24, min(6, vals.get(lvl, default) + trims[key])), 1)

# ------------------------------------------------------------------------------------------------ emit
def cpp(v):
    if isinstance(v, bool): return '1' if v else '0'
    if isinstance(v, int): return str(v)
    s = repr(float(v))
    return s + 'f'

lines = ['// Generated by scripts/gen_presets_181.py - factory presets for the 1.8.1 instruments. Do not edit by hand.',
         '// Included inside SignaturePresets::bank() (SignaturePresets.h), after SignaturePresets18.inl.']
for bank, items in banks.items():
    lines.append(f'        {{ "{bank}", {{')
    for cat, name, adv, vals, props, mods in items:
        pv = ', '.join(f'N ("{k}", {cpp(v)})' for k, v in vals.items())
        pr = ', '.join(f'{{ "{k}", {cpp(v)} }}' for k, v in props.items())
        md = ', '.join(f'{{ {s}, "{d}", {cpp(dep)} }}' for s, d, dep in mods)
        fn = 'A' if adv else 'B'
        lines.append(f'            {fn} ("{cat} | {name}", {{ {pv} }}, {{ {pr} }}, {{ {md} }}),')
    lines[-1] = lines[-1].rstrip(',')
    lines.append('        } },')
open(OUT, 'w').write('\n'.join(lines) + '\n')
for bank, items in banks.items():
    names = [f'{c} | {n}' for c, n, *_ in items]
    assert len(names) == len(set(names)), f'duplicate preset name in {bank}'
    print(f'{bank}: {len(items)} presets, {len(set(c for c, *_ in items))} categories')
print('wrote', OUT)

