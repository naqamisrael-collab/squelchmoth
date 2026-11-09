#!/usr/bin/env python3
"""Factory presets for the 1.11 Color & Space second wave, part 3 (super prompt C6, C7, C16):
   Larva Filter 2 (150), Twelve Phase 2 (130) and Hive Rack (200 chains, from Source/SqHiveChains.h).

Larva Filter 2 and Twelve Phase 2 presets are rendered through the real DSP (tests/preset_calib_d3.cpp, built from
Source/SqColor111dDsp.h) and their Output trim is written from the measurement so they hold their level on the material
they were made for, with the peak kept under +3 dBFS. Hive Rack presets load a factory chain (prop "chain") and carry the
Output trim measured in the app (scripts/gen_hive_chains.py, from the in-app preset audit).

Writes tests/presets111f_table.h and Source/SignaturePresets111f.inl (add111f(), called from SignaturePresets::bank()).
"""
import json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Wing Mod sync prop = division index + 1 (0 = free). Division indices: 3 1/32, 6 1/16, 7 1/16., 8 1/16T, 9 1/8, 10 1/8., 11 1/8T,
# 12 1/4, 13 1/4., 14 1/4T, 15 1/2, 16 1/2., 18 1 bar, 21 2 bars, 24 4 bars
D32, D16, D16D, D16T, D8, D8D, D8T, D4, D4D, D4T, D2, D2D, BAR, BAR2, BAR4 = 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 21, 24
def SY(div): return div + 1
LFO, RHY, ENV, RND, STEP, ADSR = range(6)
SINE, TRI, RUP, RDN, SQR, PUL, SH, SRND, CUST = range(9)
LP, HP, BP, NO = range(4)
TR, DI, SK, SV = range(4)          # transistor ladder, diode ladder, Sallen-Key, state-variable
SER, PAR, SPLIT = range(3)
# Tremor rhythm patterns
PAT = {n: i for i, n in enumerate(["Even 16ths", "Pulse Accents", "Offbeat Chop", "Gallop", "Tresillo", "Clave 3-2", "Clave 2-3", "Trance Gate", "Stutter End",
                                    "Swell Build", "Fade Out", "Half Time", "Quarter Pump", "Dotted Eighths", "Skip Beat", "Bounce", "Morse", "Ghost Notes",
                                    "Shuffle", "Broken", "Sparse", "Rolling", "Afro Bell", "Amen Chop"])}
WM_PROPS = ['modMode', 'modShape', 'modSync', 'modSmooth', 'modTrigger', 'numSteps', 'stepDivision', 'pattern']

PLUGS = {}
def plug(name, values, props): PLUGS[name] = (values, props, [])
def add(name, preset, vals, props):
    values, propids, lst = PLUGS[name]
    for k in vals: assert k in values, (name, k)
    for k in props: assert k in propids, (name, k)
    assert ' | ' in preset, preset
    lst.append(dict(name=preset, vals=vals, props=props))

#=====================================================================================================================
# Larva Filter 2
LV = ['cutoff', 'resonance', 'freqMod', 'resMod', 'lrOffset', 'drive', 'mix', 'output', 'input'] + ['s%d' % i for i in range(1, 9)] + \
     ['modRate', 'groove', 'feel', 'envAttack', 'envRelease', 'envSens', 'adsrA', 'adsrD', 'adsrS', 'adsrR']
LPR = ['stages', 'routing'] + ['resp%d' % i for i in range(1, 9)] + ['model%d' % i for i in range(1, 9)] + ['analog', 'analogPostOn', 'keyOn'] + WM_PROPS
plug('Larva Filter 2', LV, LPR)
def L(name, cut, res, types, mode=LFO, shape=SINE, rate=1.0, sync=0, fm=2.0, rm=0, lr=0.0, mix=100, offs=None, routing=SER, analog=0, drive=50, post=0,
      smooth=0, trig=0, steps=16, stepdiv=D16, pattern=1, groove=0, feel=0, ea=10, er=150, es=0, a=5, d=150, s=60, r=300, inp=0):
    """types: list of (response, model) per stage; the stage count is its length."""
    n = len(types)
    vals = dict(cutoff=cut, resonance=res, freqMod=fm, resMod=rm, lrOffset=lr, drive=drive, mix=mix, input=inp, modRate=rate, groove=groove, feel=feel,
                envAttack=ea, envRelease=er, envSens=es, adsrA=a, adsrD=d, adsrS=s, adsrR=r)
    for i, o in enumerate(offs or []): vals['s%d' % (i + 1)] = o
    props = dict(stages=n, routing=routing, analog=analog, analogPostOn=post, keyOn=0, modMode=mode, modShape=shape, modSync=sync, modSmooth=smooth,
                 modTrigger=trig, numSteps=steps, stepDivision=stepdiv, pattern=pattern)
    for i in range(8):
        rsp, mdl = types[i] if i < n else (LP, i % 4)
        props['resp%d' % (i + 1)] = rsp; props['model%d' % (i + 1)] = mdl
    add('Larva Filter 2', name, vals, props)

# Sweeps (20)
L("Sweeps | Slow Ladder Sweep", 700, 45, [(LP, TR), (LP, TR)], rate=0.2, fm=3)
L("Sweeps | Bar Sweep Lowpass", 900, 50, [(LP, TR), (LP, TR)], sync=SY(BAR), fm=3, shape=TRI)
L("Sweeps | Two Bar Rise", 400, 40, [(LP, DI), (LP, DI)], sync=SY(BAR2), shape=RUP, fm=4)
L("Sweeps | Four Bar Build", 1200, 35, [(LP, TR), (LP, TR)], sync=SY(BAR4), shape=RUP, fm=3, smooth=2)
L("Sweeps | Quarter Wobble", 600, 60, [(LP, SK)], sync=SY(D4), fm=2.5)
L("Sweeps | Eighth Wobble", 650, 62, [(LP, SK)], sync=SY(D8), fm=2.5)
L("Sweeps | Triplet Wobble", 700, 58, [(LP, DI), (LP, DI)], sync=SY(D8T), fm=2.2)
L("Sweeps | Highpass Riser", 120, 35, [(HP, SV), (HP, SV)], sync=SY(BAR2), shape=RUP, fm=4)
L("Sweeps | Highpass Fall", 2500, 30, [(HP, SV)], sync=SY(BAR), shape=RDN, fm=3)
L("Sweeps | Bandpass Glide", 1000, 55, [(BP, SV)], rate=0.35, fm=3, mix=85)
L("Sweeps | Notch Drift", 1200, 30, [(NO, SV), (NO, SV)], rate=0.15, fm=2.5, offs=[0, 0.6])
L("Sweeps | Stereo Swirl", 900, 45, [(LP, TR), (BP, SV)], rate=0.3, fm=2.5, lr=0.8)
L("Sweeps | Sample Hold Steps", 1100, 55, [(LP, DI)], sync=SY(D16), shape=SH, fm=2.5)
L("Sweeps | Smooth Random", 900, 45, [(LP, TR), (LP, TR)], sync=SY(D4), shape=SRND, fm=2.5, smooth=2)
L("Sweeps | Square Jump", 800, 50, [(LP, SK)], sync=SY(D8), shape=SQR, fm=2, smooth=1)
L("Sweeps | Glide Square", 800, 50, [(LP, TR), (LP, TR)], sync=SY(D4), shape=SQR, fm=2.5, smooth=4)
L("Sweeps | Dub Siren Filter", 600, 70, [(LP, DI), (BP, SK)], sync=SY(D8D), fm=3, analog=2, drive=30)
L("Sweeps | Wide Morph", 1500, 40, [(LP, SV), (BP, SV), (HP, SV), (NO, SV)], routing=PAR, rate=0.25, fm=2, lr=0.5, offs=[0, 0.5, -1, 1])
L("Sweeps | Resonant Peak Ride", 1000, 75, [(BP, TR)], rate=0.12, fm=4, mix=80)
L("Sweeps | Ladder Crawl", 250, 55, [(LP, TR), (LP, TR), (LP, DI)], rate=0.05, fm=4.5, analog=2, drive=25)
# Wah & Envelope (20)
L("Wah & Envelope | Auto Wah Funk", 500, 65, [(BP, SV)], mode=ENV, fm=3.5, ea=4, er=120, es=6)
L("Wah & Envelope | Quack Bandpass", 400, 72, [(BP, SK)], mode=ENV, fm=4, ea=2, er=90, es=8)
L("Wah & Envelope | Envelope Ladder", 300, 55, [(LP, TR), (LP, TR)], mode=ENV, fm=4, ea=5, er=200, es=6)
L("Wah & Envelope | Slow Bloom", 300, 40, [(LP, TR)], mode=ENV, fm=4, ea=80, er=600, es=6)
L("Wah & Envelope | Snappy Funk Keys", 600, 60, [(BP, SV), (LP, SV)], mode=ENV, fm=3, ea=1, er=70, es=10)
L("Wah & Envelope | Down Wah", 3000, 55, [(LP, DI)], mode=ENV, fm=-3, ea=3, er=150, es=8)
L("Wah & Envelope | Touch Highpass", 80, 40, [(HP, SV)], mode=ENV, fm=4, ea=3, er=180, es=6)
L("Wah & Envelope | Talk Box Vowels", 700, 70, [(BP, SV), (BP, SV)], routing=PAR, mode=ENV, fm=2.5, offs=[0, 1.4], ea=6, er=160, es=6)
L("Wah & Envelope | ADSR Pluck", 700, 50, [(LP, TR), (LP, TR)], mode=ADSR, fm=3.5, a=1, d=180, s=10, r=150)
L("Wah & Envelope | ADSR Swell", 300, 40, [(LP, TR)], mode=ADSR, fm=4, a=400, d=500, s=70, r=600)
L("Wah & Envelope | ADSR Blip", 500, 65, [(BP, SK)], mode=ADSR, fm=4, a=0.5, d=60, s=0, r=80)
L("Wah & Envelope | Retriggered Snap", 400, 60, [(LP, DI)], mode=ADSR, fm=4.5, a=1, d=110, s=20, r=100)
L("Wah & Envelope | Envelope Notch", 600, 40, [(NO, SV)], mode=ENV, fm=3, ea=5, er=220, es=6)
L("Wah & Envelope | Gentle Follower", 800, 30, [(LP, SV)], mode=ENV, fm=2, ea=10, er=300, es=4)
L("Wah & Envelope | Screaming Wah", 600, 85, [(BP, SK)], mode=ENV, fm=3, ea=4, er=140, es=8, analog=4, drive=35)
L("Wah & Envelope | Fuzz Wah", 550, 70, [(BP, SV)], mode=ENV, fm=3, ea=3, er=130, es=6, analog=6, drive=40)
L("Wah & Envelope | Parallel Wah", 500, 60, [(BP, SV), (BP, SV)], routing=PAR, mode=ENV, fm=3, offs=[0, 1.2], ea=4, er=150, es=6)
L("Wah & Envelope | Inverse Envelope", 4000, 45, [(LP, TR), (LP, TR)], mode=ENV, fm=-4, ea=8, er=250, es=6)
L("Wah & Envelope | Slow Attack Open", 400, 45, [(LP, DI), (LP, DI)], mode=ENV, fm=3.5, ea=150, er=400, es=6)
L("Wah & Envelope | Stereo Envelope", 500, 55, [(BP, SV)], mode=ENV, fm=3, lr=0.6, ea=4, er=150, es=6)
# Rhythmic (20) - drum loop target
for i, (pat, div, cut, res, typ, fm) in enumerate([
        ("Even 16ths", D16, 500, 55, (LP, TR), 3), ("Pulse Accents", D16, 600, 60, (LP, DI), 3), ("Offbeat Chop", D16, 700, 50, (BP, SV), 2.5),
        ("Gallop", D16, 550, 55, (LP, TR), 3), ("Tresillo", D16, 450, 60, (LP, SK), 3.5), ("Clave 3-2", D16, 600, 55, (LP, SK), 2.5),
        ("Trance Gate", D16, 400, 50, (LP, TR), 4), ("Half Time", D8, 500, 55, (LP, DI), 3), ("Quarter Pump", D16, 350, 45, (LP, TR), 4),
        ("Dotted Eighths", D16, 650, 60, (LP, SK), 3), ("Skip Beat", D16, 600, 55, (LP, SV), 2.5), ("Bounce", D16, 500, 60, (LP, TR), 3),
        ("Morse", D32, 800, 50, (BP, SV), 2), ("Ghost Notes", D16, 500, 50, (LP, DI), 3), ("Shuffle", D16, 550, 55, (LP, TR), 3),
        ("Broken", D16, 700, 60, (BP, SK), 2.5), ("Rolling", D32, 600, 55, (LP, TR), 3), ("Afro Bell", D16, 900, 55, (BP, SV), 2.5),
        ("Amen Chop", D16, 500, 55, (LP, DI), 3), ("Stutter End", D16, 600, 50, (LP, TR), 3)]):
    L("Rhythmic | %s Filter" % pat, cut, res, [typ, typ], mode=RHY, fm=fm, sync=SY(div), stepdiv=div, pattern=PAT[pat],
      groove=30 if pat in ("Shuffle", "Ghost Notes", "Gallop") else 0, smooth=1)
# Vocal (16)
L("Vocal | Telephone Line", 1400, 20, [(HP, SV), (LP, SV)], offs=[-1.6, 0.9], mode=LFO, rate=0.1, fm=0)
L("Vocal | Radio Voice", 1600, 25, [(HP, SK), (LP, SK)], offs=[-1.8, 1.4], fm=0, analog=4, drive=20)
L("Vocal | Megaphone", 1500, 35, [(BP, SK), (HP, SV)], offs=[0, -1.5], fm=0, analog=5, drive=35)
L("Vocal | Breathy Air Lift", 6000, 15, [(HP, SV)], offs=[0], fm=0, mix=35)
L("Vocal | Warm Darken", 5000, 10, [(LP, TR)], fm=0, mix=100)
L("Vocal | Slow Vowel Morph", 900, 55, [(BP, SV), (BP, SV)], offs=[0, 1.3], rate=0.12, fm=1.2, mix=55)
L("Vocal | Hook Filter Rise", 400, 30, [(HP, SV)], sync=SY(BAR2), shape=RUP, fm=3, mix=100)
L("Vocal | Pre-Chorus Lowpass Open", 2200, 25, [(LP, SV)], sync=SY(BAR4), shape=RUP, fm=2.5)
L("Vocal | Wah Vocal", 800, 55, [(BP, SV)], mode=ENV, fm=2, ea=10, er=200, es=0, mix=50)
L("Vocal | Robot Formant", 1100, 70, [(BP, SV), (BP, SV), (BP, SV)], routing=PAR, offs=[-1, 0, 1.4], fm=0, mix=60)
L("Vocal | Gated Vocal Chop", 2500, 20, [(LP, SV)], mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT["Offbeat Chop"], fm=3, smooth=1)
L("Vocal | Dreamy Swirl", 1800, 35, [(LP, SV), (NO, SV)], rate=0.2, fm=1.5, lr=0.5, mix=45)
L("Vocal | Lo-Fi Mic", 2000, 30, [(HP, SK), (LP, DI)], offs=[-2.5, 0.4], fm=0, analog=4, drive=25)
L("Vocal | Distant Room", 2200, 15, [(LP, TR), (HP, SV)], offs=[0, -3], fm=0)
L("Vocal | Ad-Lib Throw Filter", 700, 45, [(BP, SV)], sync=SY(D4), fm=2, mix=60)
L("Vocal | Whisper Band", 4000, 40, [(BP, SV)], fm=0, mix=50)
# Drums (16)
L("Drums | Loop Lowpass Sweep", 400, 30, [(LP, TR), (LP, TR)], sync=SY(BAR2), shape=TRI, fm=4)
L("Drums | Breakdown Muffle", 350, 25, [(LP, TR), (LP, TR)], fm=0)
L("Cleanup | Hats And Air", 2500, 20, [(HP, SV)], fm=0)
L("Cleanup | Kick Thinned", 140, 15, [(HP, SV)], fm=0)
L("Drums | Snare Ring Band", 900, 60, [(BP, SV)], fm=0, mix=45)
L("Drums | Envelope Smack", 500, 45, [(LP, DI)], mode=ENV, fm=3.5, ea=1, er=80, es=6)
L("Drums | Breakbeat Wah", 700, 55, [(BP, SK)], mode=ENV, fm=3, ea=2, er=110, es=6)
L("Drums | Sixteenth Chop LP", 600, 45, [(LP, TR)], mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT["Pulse Accents"], fm=3)
L("Drums | Crunchy Bus", 3500, 25, [(LP, DI)], fm=0, analog=5, drive=45)
L("Drums | Phone Break", 1500, 30, [(HP, SK), (LP, SK)], offs=[-1.5, 1.2], fm=0)
L("Drums | Room Notch Sweep", 1200, 40, [(NO, SV), (NO, SV)], rate=0.25, fm=2, offs=[0, 0.8])
L("Drums | Tom Resonator", 220, 70, [(BP, TR)], fm=0, mix=35)
L("Drums | Highpass Build", 60, 30, [(HP, SV), (HP, SV)], sync=SY(BAR4), shape=RUP, fm=6)
L("Drums | Pumping Lowpass", 300, 35, [(LP, TR), (LP, TR)], mode=RHY, sync=SY(D4), stepdiv=D16, pattern=PAT["Quarter Pump"], fm=4, smooth=2)
L("Drums | Dusty Sampler", 5000, 20, [(LP, DI)], fm=0, analog=4, drive=30)
L("Drums | Stereo Split Filter", 1500, 35, [(LP, SV)], fm=0, lr=1.2)
# Bass (16)
L("Bass | Acid Squelch", 350, 78, [(LP, DI)], mode=ADSR, fm=4, a=1, d=200, s=10, r=120, analog=4, drive=35)
L("Bass | Acid Eighths", 300, 75, [(LP, DI)], mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT["Rolling"], fm=4, analog=4, drive=30)
L("Bass | Round Ladder", 450, 35, [(LP, TR), (LP, TR)], fm=0)
L("Bass | Ladder Growl", 400, 60, [(LP, TR), (LP, TR)], mode=ENV, fm=3, ea=3, er=150, es=6, analog=2, drive=40)
L("Bass | Wobble Quarter", 300, 60, [(LP, TR), (LP, TR)], sync=SY(D4), fm=4)
L("Bass | Wobble Eighth Triplet", 300, 62, [(LP, DI), (LP, DI)], sync=SY(D8T), fm=4)
L("Bass | Reese Movement", 600, 40, [(LP, SV), (NO, SV)], rate=0.18, fm=1.5, lr=0.3)
L("Bass | Funk Envelope", 350, 60, [(LP, SK)], mode=ENV, fm=3.5, ea=2, er=100, es=8)
L("Bass | Sub Focus", 160, 20, [(LP, TR), (LP, TR)], fm=0)
L("Bass | Midrange Bite", 800, 45, [(BP, SV), (LP, TR)], routing=PAR, offs=[0, -0.5], fm=0)
L("Bass | Talking Bass", 500, 70, [(BP, SV), (BP, SV)], routing=PAR, offs=[0, 1.2], mode=ENV, fm=2.5, ea=5, er=140, es=6)
L("Bass | Dub Low Pass", 220, 45, [(LP, DI), (LP, DI)], rate=0.08, fm=1)
L("Bass | Driven Diode", 500, 50, [(LP, DI)], fm=0, analog=6, drive=40)
L("Bass | Sample Hold Bass", 400, 60, [(LP, TR)], sync=SY(D16), shape=SH, fm=2.5)
L("Bass | Pluck Bass", 250, 45, [(LP, TR), (LP, TR)], mode=ADSR, fm=4, a=1, d=120, s=15, r=100)
L("Bass | Growl Notch Pair", 700, 50, [(LP, TR), (NO, SV)], rate=2.5, fm=1.5)
# Synth (18)
L("Synth | Pad Slow Open", 500, 30, [(LP, TR), (LP, TR)], sync=SY(BAR4), shape=TRI, fm=3)
L("Synth | Saw Lead Bite", 1800, 55, [(LP, DI)], mode=ADSR, fm=2.5, a=2, d=250, s=40, r=200)
L("Synth | Step Sequence Filter", 700, 65, [(LP, DI)], mode=STEP, sync=SY(D16), stepdiv=D16, pattern=PAT["Broken"], fm=3, smooth=1)
L("Synth | Random Steps", 900, 60, [(LP, TR)], mode=RND, sync=SY(D16), fm=2.5)
L("Synth | Formant Pad", 900, 60, [(BP, SV), (BP, SV), (BP, SV)], routing=PAR, offs=[-1, 0.3, 1.5], rate=0.1, fm=1)
L("Synth | Arp Sixteenths", 800, 55, [(LP, SK)], mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT["Even 16ths"], fm=2.5)
L("Synth | Trance Gate Filter", 600, 40, [(LP, TR)], mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT["Trance Gate"], fm=4)
L("Synth | Analog Brass", 900, 35, [(LP, TR), (LP, TR)], mode=ADSR, fm=2.5, a=40, d=300, s=60, r=250)
L("Synth | Glassy Highpass", 900, 50, [(HP, SV), (BP, SV)], offs=[0, 1.5], rate=0.2, fm=1)
L("Synth | Comb Morph", 1000, 60, [(NO, SV), (NO, SV), (NO, SV), (NO, SV)], offs=[0, 0.5, 1, 1.5], rate=0.15, fm=1.5)
L("Synth | Screamer Lead", 1200, 80, [(LP, SK)], rate=5, fm=0.5, analog=4, drive=35)
L("Synth | Wide Stereo Filter", 1200, 45, [(LP, SV)], rate=0.3, fm=2, lr=1.0)
L("Synth | 4 Plus 4 Morph", 900, 50, [(LP, TR), (LP, TR), (LP, TR), (LP, TR), (HP, SV), (BP, SV), (BP, SV), (NO, SV)], routing=SPLIT,
  offs=[0, 0, 0, 0, 1, 1.5, 2, 0.5], rate=0.2, fm=2)
L("Synth | Pulse Accent Pad", 800, 45, [(LP, TR)], mode=RHY, sync=SY(D8), stepdiv=D8, pattern=PAT["Pulse Accents"], fm=3, smooth=3)
L("Synth | Vibrato Filter", 1400, 50, [(LP, SV)], rate=6, fm=0.4)
L("Synth | Keys Warmth", 3000, 20, [(LP, TR)], fm=0, analog=2, drive=25)
L("Synth | Crystal Bandpass", 2500, 70, [(BP, SV), (BP, SV)], routing=PAR, offs=[0, 1], rate=0.1, fm=1, mix=60)
L("Synth | Plucked String", 900, 65, [(LP, SK)], mode=ADSR, fm=3.5, a=0.5, d=90, s=0, r=90)
# Lo-Fi (12)
L("Lo-Fi | AM Radio", 1800, 30, [(HP, SK), (LP, DI)], offs=[-1.8, 0.8], fm=0, analog=5, drive=30)
L("Lo-Fi | Cassette Dull", 3500, 15, [(LP, TR), (LP, TR)], rate=0.6, fm=0.08, analog=2, drive=25)
L("Lo-Fi | Walkie Talkie", 1700, 50, [(BP, SK), (BP, SK)], fm=0, analog=6, drive=45)
L("Lo-Fi | Old Gramophone", 1500, 35, [(HP, SV), (LP, SK)], offs=[-1.2, 1.0], fm=0, analog=4, drive=35)
L("Lo-Fi | Underwater", 300, 40, [(LP, TR), (LP, TR)], rate=0.3, fm=0.6)
L("Lo-Fi | Next Room Party", 250, 30, [(LP, TR), (LP, TR)], fm=0)
L("Lo-Fi | Wobbly Tape", 3000, 20, [(LP, DI)], rate=0.8, fm=0.3, analog=2, drive=30)
L("Lo-Fi | Crushed Band", 1200, 55, [(BP, DI)], fm=0, analog=7, drive=60)
L("Lo-Fi | Dusty Keys", 2500, 25, [(LP, TR), (HP, SV)], offs=[0, -3.5], fm=0, analog=4, drive=25)
L("Lo-Fi | Toy Speaker", 2200, 45, [(HP, SV), (BP, SK)], offs=[-1, 0], fm=0)
L("Lo-Fi | Laptop Speaker", 1600, 20, [(HP, SV), (HP, SV)], fm=0)
L("Lo-Fi | Broken Amp", 900, 60, [(BP, SK)], fm=0, analog=6, drive=55)
# Sound Design (12)
L("Sound Design | Audio Rate Growl", 600, 60, [(LP, DI)], rate=80, fm=1.2)
L("Sound Design | Laser Zaps", 2000, 80, [(BP, SK)], mode=ADSR, fm=-5, a=0.5, d=120, s=0, r=60)
L("Sound Design | Eight Stage Comb", 500, 65, [(BP, SV)] * 8, routing=PAR, offs=[-2, -1, -0.5, 0, 0.5, 1, 1.5, 2], rate=0.1, fm=1)
L("Sound Design | Resonant Cluster", 800, 85, [(BP, SV), (BP, SV), (BP, SV), (BP, SV)], routing=PAR, offs=[0, 0.58, 1.0, 1.58], rate=0.05, fm=0.5, mix=60)
L("Sound Design | Alien Chatter", 900, 75, [(BP, SV), (BP, SV)], routing=PAR, sync=SY(D32), shape=SH, fm=3, offs=[0, 1])
L("Sound Design | Metallic Ring", 1500, 90, [(BP, SK)], rate=40, fm=0.5, mix=60)
L("Sound Design | Spin Cycle", 700, 55, [(NO, SV), (LP, TR)], rate=3.5, fm=2.5, lr=1.5)
L("Sound Design | Collapse", 8000, 50, [(LP, DI), (LP, DI)], sync=SY(BAR2), shape=RDN, fm=-6)
L("Sound Design | Random Formants", 1000, 70, [(BP, SV), (BP, SV), (BP, SV)], routing=PAR, offs=[-1, 0, 1], mode=RND, sync=SY(D8), fm=1.5, smooth=2)
L("Sound Design | Notch Machine", 1000, 40, [(NO, SV)] * 6, offs=[-2, -1.2, -0.4, 0.4, 1.2, 2], rate=0.4, fm=1)
L("Sound Design | Diode Whine", 1200, 92, [(LP, DI), (LP, DI)], rate=0.07, fm=3, mix=70)
L("Sound Design | Ping-Pong Filters", 900, 60, [(BP, SV)], sync=SY(D8), shape=SQR, fm=2, lr=2.0, smooth=1)

#=====================================================================================================================
# Twelve Phase 2. Styles are the twelve archetypes x six variations of Source/SqColor111dDsp.h (Phase2::styles()).
ARCH = [("One-Knob Amber 4", 4, 1, 0, 0.25, 700, 3.0, 0.5, 0, SINE, 0, 0), ("Script Amber 4", 4, 1, 0, 0.10, 650, 2.8, 0.4, 0, SINE, 0, 0),
        ("Optical Vibe 6", 6, 1, 0, 0.05, 900, 2.2, 4.5, 20, TRI, 0, 0), ("Pebble 4", 4, 1, 1, 0.55, 800, 3.2, 0.3, 0, TRI, 0, 0),
        ("Twin Phase Rack 6", 6, 1, 0, 0.40, 600, 3.5, 0.2, 90, SINE, 0, 0), ("Synth 12", 12, 0, 0, 0.60, 1000, 4.0, 0.15, 0, TRI, 0, 0),
        ("Dual Rack 8", 8, 0, 1, 0.45, 750, 3.0, 0.25, 180, SINE, 0, 0), ("Barber Pole 10", 10, 0, 0, 0.30, 1200, 4.5, 0.08, 120, RUP, 0, 0),
        ("Through-Zero 8", 8, 0, 0, 0.20, 900, 3.0, 0.2, 0, SINE, 1, 1), ("Deep 24", 24, 0, 0, 0.50, 900, 4.0, 0.1, 45, SINE, 0, 0),
        ("Notch Sweep 2", 2, 0, 0, 0.00, 1000, 3.5, 0.3, 0, SINE, 0, 1), ("Odd Seven", 7, 1, 1, 0.35, 850, 3.0, 0.35, 60, TRI, 0, 0)]
VARS = ["", " Slow", " Fast", " Deep", " Subtle", " Wide"]
STYLES = []
for a in ARCH:
    for k, v in enumerate(VARS):
        name, st, cl, rm, res, cen, sw, rate, spr, shp, tz, pol = a
        if k == 1: rate *= 0.4
        if k == 2: rate *= 2.8
        if k == 3: res = min(0.85, res + 0.3); sw += 1.0
        if k == 4: res *= 0.4; sw *= 0.5
        if k == 5: spr = min(180.0, spr + 90.0)
        STYLES.append(dict(name=name + v, stages=st, classic=cl, resMode=rm, res=res, centre=cen, sweep=sw, rate=rate, spread=spr, shape=shp, tz=tz, pol=pol))
SIDX = {s['name']: i for i, s in enumerate(STYLES)}
PV = ['centre', 'sweep', 'resonance', 'depth', 'spread', 'mix', 'output', 'drive', 'modRate', 'groove', 'feel', 'envAttack', 'envRelease', 'envSens',
      'adsrA', 'adsrD', 'adsrS', 'adsrR']
PPR = ['style', 'stages', 'classicOn', 'resMode', 'polarity', 'tzOn', 'analog', 'analogPostOn', 'keyOn'] + WM_PROPS
plug('Twelve Phase 2', PV, PPR)
def P(name, style, rate=None, sync=0, res=None, centre=None, sweep=None, spread=None, depth=100, mix=100, mode=LFO, shape=None, stages=None, analog=0, drive=50,
      post=0, smooth=0, pattern=1, stepdiv=D16, steps=16, groove=0, feel=0, ea=10, er=200, es=0, a=5, d=150, s=60, r=300, tz=None, pol=None, resMode=None, trig=0):
    st = STYLES[SIDX[style]]
    vals = dict(centre=centre if centre is not None else st['centre'], sweep=sweep if sweep is not None else st['sweep'],
                resonance=round((res if res is not None else st['res'] * 100), 1), depth=depth, spread=spread if spread is not None else st['spread'], mix=mix,
                drive=drive, modRate=round(rate if rate is not None else st['rate'], 3), groove=groove, feel=feel, envAttack=ea, envRelease=er, envSens=es,
                adsrA=a, adsrD=d, adsrS=s, adsrR=r)
    props = dict(style=SIDX[style], stages=stages or st['stages'], classicOn=st['classic'], resMode=st['resMode'] if resMode is None else resMode,
                 polarity=st['pol'] if pol is None else pol, tzOn=st['tz'] if tz is None else tz, analog=analog, analogPostOn=post, keyOn=0,
                 modMode=mode, modShape=st['shape'] if shape is None else shape, modSync=sync, modSmooth=smooth, modTrigger=trig, numSteps=steps,
                 stepDivision=stepdiv, pattern=pattern)
    add('Twelve Phase 2', name, vals, props)

# Classic (24): every archetype, as designed and slow / deep
for a in ARCH:
    P("Classic | %s" % a[0], a[0])
    P("Classic | %s Deep Slow" % a[0], a[0] + " Deep", rate=a[7] * 0.5)
# Guitar (14)
P("Guitar | Funk Rhythm Phase", "One-Knob Amber 4", rate=0.9)
P("Guitar | Classic Rock Lead", "Script Amber 4 Slow")
P("Guitar | Clean Chorus-ish Phase", "Script Amber 4 Subtle", rate=0.6)
P("Guitar | Quarter Note Swirl", "One-Knob Amber 4", sync=SY(D4))
P("Guitar | Eighth Note Pulse", "Pebble 4 Fast", sync=SY(D8))
P("Guitar | Vibe Rotor Wash", "Optical Vibe 6", rate=5.5)
P("Guitar | Vibe Slow Throb", "Optical Vibe 6 Slow")
P("Guitar | Jet Sweep Solo", "Pebble 4 Deep", rate=0.2)
P("Guitar | Shoegaze Wall", "Synth 12 Deep", rate=0.12, mix=80, analog=2, drive=25)
P("Guitar | Twin Rack Stereo", "Twin Phase Rack 6 Wide")
P("Guitar | Fuzz Into Phase", "One-Knob Amber 4 Deep", analog=6, drive=35)
P("Guitar | Acoustic Shimmer", "Notch Sweep 2 Subtle", rate=0.25, mix=50)
P("Guitar | Odd Seven Chime", "Odd Seven", rate=0.45)
P("Guitar | Envelope Phase Funk", "Pebble 4", mode=ENV, ea=3, er=150, es=6)
# Keys (14)
P("Keys | Electric Piano Swirl", "Script Amber 4", rate=0.5)
P("Keys | Tine Vibe", "Optical Vibe 6 Subtle", rate=3.5)
P("Keys | Funk Keys Phase", "One-Knob Amber 4 Fast")
P("Keys | Organ Rotor-ish", "Optical Vibe 6 Fast", rate=6.5, mix=80)
P("Keys | Stereo Tines", "Twin Phase Rack 6 Wide", rate=0.35)
P("Keys | Synth Strings Ensemble", "Synth 12 Wide", rate=0.2)
P("Keys | Pad Glide", "Deep 24 Subtle", rate=0.08)
P("Keys | Half Bar Phase", "Dual Rack 8", sync=SY(D2))
P("Keys | Reed Piano Throb", "Pebble 4 Subtle", rate=0.8)
P("Keys | Dotted Phase", "One-Knob Amber 4", sync=SY(D8D))
P("Keys | Lush Odd", "Odd Seven Wide", rate=0.3)
P("Keys | Through-Zero Keys", "Through-Zero 8 Subtle", rate=0.25)
P("Keys | Barber Pole Rise", "Barber Pole 10", rate=0.06)
P("Keys | Vintage Warm Phase", "Script Amber 4", analog=2, drive=30)
# Synth (14)
P("Synth | Classic Synth Sweep", "Synth 12", rate=0.15)
P("Synth | Resonant Sweep", "Synth 12 Deep", res=75)
P("Synth | Bar Sweep", "Synth 12", sync=SY(BAR))
P("Synth | Two Bar Sweep", "Deep 24", sync=SY(BAR2))
P("Synth | Barber Pole Endless", "Barber Pole 10 Wide")
P("Synth | Barber Pole Fall", "Barber Pole 10", shape=RDN)
P("Synth | Sample Hold Phase", "Dual Rack 8", sync=SY(D16), shape=SH)
P("Synth | Random Drift", "Synth 12", sync=SY(D4), shape=SRND, smooth=2)
P("Synth | Arp Step Phase", "Dual Rack 8", mode=STEP, sync=SY(D16), stepdiv=D16, pattern=PAT["Broken"], smooth=1)
P("Synth | Inside Resonance", "Dual Rack 8 Deep")
P("Synth | Negative Polarity", "Synth 12", pol=1)
P("Synth | 16 Stage Lush", "Synth 12 Wide", stages=16)
P("Synth | Square Jump", "Synth 12", sync=SY(D4), shape=SQR, smooth=1)
P("Synth | ADSR Phase Pluck", "Synth 12", mode=ADSR, a=1, d=300, s=10, r=300)
# Vocal (12)
P("Vocal | Subtle Width", "Twin Phase Rack 6 Subtle", mix=40)
P("Vocal | Dreamy Lead", "Script Amber 4 Slow", mix=45)
P("Vocal | Background Swirl", "Synth 12 Wide", mix=55)
P("Vocal | Psychedelic Verse", "One-Knob Amber 4 Deep", mix=60)
P("Vocal | Robotic Comb", "Notch Sweep 2 Fast", mix=60)
P("Vocal | Flanged-ish Hook", "Through-Zero 8", mix=60)
P("Vocal | Throat Vibe", "Optical Vibe 6", mix=50)
P("Vocal | Bar Swirl", "Dual Rack 8 Wide", sync=SY(BAR), mix=50)
P("Vocal | Ad-Lib Phase", "Pebble 4 Fast", mix=55)
P("Vocal | Envelope Phase", "Pebble 4", mode=ENV, ea=5, er=200, es=0, mix=50)
P("Vocal | Deep Space Voice", "Deep 24 Wide", mix=45)
P("Vocal | Odd Harmonic Halo", "Odd Seven Subtle", mix=50)
# Drums (12)
P("Drums | Hi-Hat Jet", "Pebble 4 Deep", rate=0.25)
P("Drums | Loop Swirl", "Synth 12", sync=SY(BAR))
P("Drums | Breakbeat Phase", "One-Knob Amber 4", sync=SY(D2))
P("Drums | Through-Zero Drums", "Through-Zero 8", sync=SY(BAR2))
P("Drums | Room Phase", "Twin Phase Rack 6", mix=60)
P("Drums | Envelope Hits", "Synth 12", mode=ENV, ea=1, er=90, es=6)
P("Drums | Snare Ring Phase", "Notch Sweep 2", rate=0.5, mix=60)
P("Drums | Wide Overheads", "Twin Phase Rack 6 Wide", mix=50)
P("Drums | Barber Pole Build", "Barber Pole 10", sync=SY(BAR4))
P("Drums | Crunchy Phase", "Pebble 4", analog=5, drive=40)
P("Drums | Deep Stereo Loop", "Deep 24 Wide", sync=SY(BAR2))
P("Drums | Vibe Kit", "Optical Vibe 6", mix=60)
# Rhythmic (12) - drum loop target
for pat, style in [("Even 16ths", "Synth 12"), ("Pulse Accents", "Pebble 4"), ("Offbeat Chop", "One-Knob Amber 4"), ("Gallop", "Dual Rack 8"),
                   ("Tresillo", "Odd Seven"), ("Clave 3-2", "Script Amber 4"), ("Trance Gate", "Synth 12 Deep"), ("Dotted Eighths", "Twin Phase Rack 6"),
                   ("Shuffle", "Pebble 4"), ("Afro Bell", "Odd Seven"), ("Amen Chop", "Synth 12"), ("Rolling", "Deep 24")]:
    P("Rhythmic | %s Phase" % pat, style, mode=RHY, sync=SY(D16), stepdiv=D16, pattern=PAT[pat], smooth=1, groove=30 if pat in ("Shuffle", "Gallop") else 0)
# Through-Zero (8)
for v in VARS:
    P("Through-Zero | Through-Zero 8%s" % v, "Through-Zero 8" + v)
P("Through-Zero | Bar Zero", "Through-Zero 8", sync=SY(BAR))
P("Through-Zero | Two Bar Zero", "Through-Zero 8 Deep", sync=SY(BAR2))
# Deep Space (10)
P("Deep Space | Endless Twenty-Four", "Deep 24 Slow")
P("Deep Space | Nebula", "Deep 24 Wide", rate=0.05)
P("Deep Space | Gravity Well", "Deep 24 Deep", rate=0.04)
P("Deep Space | Barber Pole Ascend", "Barber Pole 10 Slow")
P("Deep Space | Barber Pole Descend", "Barber Pole 10 Slow", shape=RDN)
P("Deep Space | Four Bar Orbit", "Deep 24", sync=SY(BAR4))
P("Deep Space | Wide Twelve", "Synth 12 Wide", rate=0.06)
P("Deep Space | Slow Dual Rack", "Dual Rack 8 Slow")
P("Deep Space | Drifting Random", "Deep 24", shape=SRND, rate=0.2, smooth=3)
P("Deep Space | Long Notch Glide", "Notch Sweep 2 Slow", stages=4)
# Sound Design (10)
P("Sound Design | Audio Rate Phase", "Synth 12", rate=60, sweep=1)
P("Sound Design | Metallic Robot", "Deep 24 Fast", rate=12, res=85)
P("Sound Design | Max Resonance Scream", "Synth 12 Deep", res=100, mix=70)
P("Sound Design | Sample Hold Chaos", "Deep 24", sync=SY(D32), shape=SH)
P("Sound Design | Zero Crusher", "Through-Zero 8 Fast", analog=7, drive=55)
P("Sound Design | Twenty-Four Inside", "Deep 24 Deep", resMode=1)
P("Sound Design | Laser Pluck", "Synth 12", mode=ADSR, a=0.5, d=120, s=0, r=60, sweep=7)
P("Sound Design | Comb Teeth", "Notch Sweep 2 Deep", stages=24, rate=0.15)
P("Sound Design | Pulse Machine", "Dual Rack 8", sync=SY(D16), shape=PUL)
P("Sound Design | Stereo Spinner", "Synth 12 Wide", rate=3, spread=180)

#=====================================================================================================================
WANT = {'Larva Filter 2': 150, 'Twelve Phase 2': 130}
CPP = {'Larva Filter 2': 'larva', 'Twelve Phase 2': 'phase'}

def specs_defaults():
    src = open(os.path.join(ROOT, 'Source', 'SqColor111d.h')).read()
    d = {}
    for cls, nm in [('SqLarvaFilter2Plugin', 'Larva Filter 2'), ('SqTwelvePhase2Plugin', 'Twelve Phase 2')]:
        body = src[src.index('class ' + cls):]
        body = body[:body.index('return s;')]
        d[nm] = {m.group(1): float(m.group(2).rstrip('f')) for m in re.finditer(r'\{ "(\w+)",\s*"[^"]+",\s*[-\d.f]+,\s*[-\d.f]+,\s*([-\d.f]+),', body)}
        assert list(d[nm].keys()) == PLUGS[nm][0], (nm, list(d[nm].keys()))
    return d

def write_table(path, defs):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111f.py - do not edit.\n#pragma once\n')
        f.write('struct D3Row { const char* name; float v[32]; int pr[40]; };\n')
        for nm, (params, props, lst) in PLUGS.items():
            f.write('// %s values: %s | props: %s\n' % (nm, ' '.join(params), ' '.join(props)))
            f.write('static const D3Row %sRows[] = {\n' % CPP[nm])
            for p in lst:
                v = [p['vals'].get(k, defs[nm][k]) for k in params]
                pr = [int(p['props'].get(k, 0)) for k in props]
                f.write('    { %s, { %s }, { %s } },\n' % (json.dumps(p['name']), ', '.join('%g' % x for x in v), ', '.join(str(x) for x in pr)))
            f.write('};\n')

def calibrate():
    exe = '/tmp/preset_calib_d3'
    src = os.path.join(ROOT, 'tests', 'preset_calib_d3.cpp')
    subprocess.check_call(['g++', '-O2', '-std=c++17', '-I' + os.path.join(ROOT, 'Source'), '-I' + os.path.join(ROOT, 'tests'), src, '-o', exe])
    out = subprocess.run([exe], text=True, capture_output=True, check=True)
    open('/tmp/calib_d3.log', 'w').write(out.stderr)
    trims = {}
    for line in out.stdout.splitlines():
        if line.startswith('TRIM\t'):
            _, plug_, name, trim, peak = line.split('\t')
            trims[(plug_, name)] = (float(trim), float(peak))
    return trims

def num(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def hive_presets():
    """Hive Rack: one preset per factory chain (prop chain = index), Output = the trim the in-app audit measured."""
    path = os.path.join(ROOT, 'scripts', 'hive_chains.json')
    if not os.path.exists(path): return []
    chains = json.load(open(path))
    trims = {}
    tp = os.path.join(ROOT, 'scripts', 'hive_trims.json')
    if os.path.exists(tp): trims = json.load(open(tp))
    out = []
    for i, c in enumerate(chains):
        out.append('        B (%s, { N ("output", %s) },\n            { { "chain", %d }, { "routing", %d } }),' % (json.dumps(c['name']), num(trims.get(c['name'], 0.0)), i, c['routing']))
    return out

def main():
    for nm, (params, props, lst) in PLUGS.items():
        names = [p['name'] for p in lst]
        dup = set(n for n in names if names.count(n) > 1)
        assert not dup, ('duplicate preset names', nm, dup)
        cats = sorted(set(n.split(' | ')[0] for n in names))
        print('%s: %d presets (want %d) in %d categories' % (nm, len(lst), WANT[nm], len(cats)))
        assert len(lst) == WANT[nm], nm
    defs = specs_defaults()
    write_table(os.path.join(ROOT, 'tests', 'presets111f_table.h'), defs)
    trims = calibrate()
    fns, adds = [], []
    worst = 0
    for k, (nm, (params, props, lst)) in enumerate(PLUGS.items()):
        lines = []
        for p in lst:
            trim, peak = trims[(nm, p['name'])]
            trim = min(trim, 3.0 - peak)
            trim = max(-24.0, min(24.0 if nm == 'Larva Filter 2' else 12.0, trim)); worst = max(worst, abs(trim))
            vals = ', '.join('N (%s, %s)' % (json.dumps(kk), num(vv)) for kk, vv in p['vals'].items()) + ', N ("output", %s)' % num(round(trim, 2))
            prs = ', '.join('{ "%s", %d }' % (kk, int(vv)) for kk, vv in p['props'].items())
            lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111f_%d()   // %s\n{\n    return {\n%s\n    };\n}\n' % (k + 1, nm, '\n'.join(lines)))
        adds.append('    m[%s] = presets111f_%d();\n' % (json.dumps(nm), k + 1))
    hv = hive_presets()
    if hv:
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111f_3()   // Hive Rack\n{\n    return {\n%s\n    };\n}\n' % '\n'.join(hv))
        adds.append('    m["Hive Rack"] = presets111f_3();\n')
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111f.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111f.py - factory presets for the 1.11 Color & Space second wave (part 3). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111e.inl; bank() calls add111f().\n')
        f.write('// Output = the measured trim that holds the level on the preset\'s material (vocal / drums / pink noise).\n\n')
        f.write('\n'.join(fns))
        f.write('inline void add111f (std::map<juce::String, std::vector<Preset>>& m)\n{\n' + ''.join(adds) + '}\n')
    print('wrote', out, '- largest trim %.2f dB, %d Hive Rack chains' % (worst, len(hv)))

if __name__ == '__main__':
    main()
