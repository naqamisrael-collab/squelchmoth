#!/usr/bin/env python3
"""Factory presets for the 1.11 Voice second wave, part 2 (super prompt V4, V14, V16):
   Hive Choir (120), Double Wing 2 (60), Mutant (60).

Rendered through the real DSP (tests/preset_calib_e2.cpp, JUCE core via scripts/build_voice111e_test.sh) on the audit
signals; each preset's Output trim holds its level on the sung vocal, peak under +3 dBFS.
Writes tests/presets111h_table.h and Source/SignaturePresets111h.inl (add111h()).
"""
import json, os, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGS = {}
def plug(name, values, props, defaults): PLUGS[name] = (values, props, [], defaults)
def add(name, preset, vals, props):
    values, propids, lst, _ = PLUGS[name]
    for k in vals: assert k in values, (name, k)
    for k in props: assert k in propids, (name, k)
    assert ' | ' in preset, preset
    lst.append(dict(name=preset, vals=vals, props=props))

#=====================================================================================================================
# Hive Choir: per voice v1..v4 Interval Level Pan Throat Unison PitchVar TimeVar Timbre Spread VibRate VibDepth, then globals
OFF, INT, STEP, CHORD, OMNI = range(5)
VK = ['Interval', 'Level', 'Pan', 'Throat', 'Unison', 'PitchVar', 'TimeVar', 'Timbre', 'Spread', 'VibRate', 'VibDepth']
HV = ['v%d%s' % (k, x) for k in range(1, 5) for x in VK] + ['lead', 'humanPitch', 'humanTime', 'consonants', 'width', 'mix', 'output']
HD = {}
for k, (iv, pan) in enumerate([(4, -50), (7, 50), (-5, -25), (12, 25)]):
    HD.update({'v%dInterval' % (k + 1): iv, 'v%dLevel' % (k + 1): -4, 'v%dPan' % (k + 1): pan, 'v%dThroat' % (k + 1): 0, 'v%dUnison' % (k + 1): 1, 'v%dPitchVar' % (k + 1): 8,
               'v%dTimeVar' % (k + 1): 15, 'v%dTimbre' % (k + 1): 30, 'v%dSpread' % (k + 1): 50, 'v%dVibRate' % (k + 1): 5.2, 'v%dVibDepth' % (k + 1): 0})
HD.update(lead=0, humanPitch=6, humanTime=12, consonants=35, width=100, mix=100, output=0)
HP = ['mode1', 'mode2', 'mode3', 'mode4', 'engine', 'voiceType', 'autoKeyOn', 'keyRoot', 'scale', 'seed']
plug('Hive Choir', HV, HP, HD)
def HC(name, voices, lead=0, hp=6, ht=12, cons=35, width=100, eng=1, vt=1, scale=1, seed=1):
    """voices: up to 4 tuples (mode, interval, level, pan, throat, unison, pitchVar, timeVar, timbre, spread, vibDepth)."""
    vals = dict(lead=lead, humanPitch=hp, humanTime=ht, consonants=cons, width=width)
    props = dict(engine=eng, voiceType=vt, autoKeyOn=1, keyRoot=0, scale=scale, seed=seed)
    for k in range(4):
        if k < len(voices):
            m, iv, lv, pan, th, un, pv, tv, tb, sp, vd = voices[k]
            props['mode%d' % (k + 1)] = m
            vals.update({'v%dInterval' % (k + 1): iv, 'v%dLevel' % (k + 1): lv, 'v%dPan' % (k + 1): pan, 'v%dThroat' % (k + 1): th, 'v%dUnison' % (k + 1): un,
                         'v%dPitchVar' % (k + 1): pv, 'v%dTimeVar' % (k + 1): tv, 'v%dTimbre' % (k + 1): tb, 'v%dSpread' % (k + 1): sp, 'v%dVibDepth' % (k + 1): vd})
        else: props['mode%d' % (k + 1)] = OFF
    add('Hive Choir', name, vals, props)
def V(m, iv, lv=-5, pan=0, th=0, un=1, pv=8, tv=15, tb=30, sp=50, vd=0): return (m, iv, lv, pan, th, un, pv, tv, tb, sp, vd)
# Pop Stacks (16)
HC("Pop Stacks | Third Above", [V(STEP, 2, -4, -40)]); HC("Pop Stacks | Third Below", [V(STEP, -2, -4, 40)])
HC("Pop Stacks | Thirds Both Sides", [V(STEP, 2, -5, -60), V(STEP, -2, -5, 60)]); HC("Pop Stacks | Third And Fifth", [V(STEP, 2, -5, -50), V(STEP, 4, -6, 50)])
HC("Pop Stacks | Wide Chorus Stack", [V(STEP, 2, -6, -80, un=4, sp=60), V(STEP, 4, -7, 80, un=4, sp=60), V(INT, 0, -8, 0, un=4, sp=90)])
HC("Pop Stacks | Hook Lift", [V(STEP, 2, -4, -50, un=2), V(INT, 12, -9, 50, un=2)]); HC("Pop Stacks | Breathy Background", [V(STEP, 2, -8, -70, th=1, un=3, tb=50), V(STEP, -3, -8, 70, th=1, un=3, tb=50)], cons=60)
HC("Pop Stacks | Tight Pop Double Third", [V(STEP, 2, -4, -30, pv=4, tv=6)], hp=3, ht=6); HC("Pop Stacks | Sixth Sweetener", [V(STEP, 5, -6, -50), V(STEP, -2, -7, 50)])
HC("Pop Stacks | Girl Group Triad", [V(STEP, 2, -5, -60, th=1), V(STEP, 4, -6, 0, th=1), V(STEP, 7, -8, 60, th=1)])
HC("Pop Stacks | Boy Band Triad", [V(STEP, -2, -5, -60, th=-1), V(STEP, -5, -6, 60, th=-1), V(STEP, 2, -7, 0)])
HC("Pop Stacks | Pre-Chorus Swell", [V(STEP, 2, -6, -50, un=3, vd=10), V(STEP, 4, -7, 50, un=3, vd=10)]); HC("Pop Stacks | Ear Candy Octave", [V(INT, 12, -9, -70, un=2), V(INT, 12, -10, 70, un=2)])
HC("Pop Stacks | R&B Stack", [V(STEP, 2, -5, -40, un=2, vd=15), V(STEP, 4, -6, 40, un=2, vd=15), V(STEP, 6, -9, 0, un=2)]); HC("Pop Stacks | Country Harmony High", [V(STEP, 2, -4, 30, pv=5)], hp=4)
HC("Pop Stacks | Bright Stack Wide", [V(STEP, 2, -6, -90, un=6, sp=70), V(STEP, 4, -7, 90, un=6, sp=70)], width=100)
# Gospel (14)
for i, (nm, vs) in enumerate([("Sunday Trio", [V(STEP, 2, -5, -50, un=4), V(STEP, 4, -6, 50, un=4)]), ("Mass Choir", [V(STEP, 2, -6, -60, un=12, sp=80), V(STEP, 4, -6, 60, un=12, sp=80), V(STEP, -3, -8, 0, un=12, sp=90)]),
                              ("Alto Line", [V(STEP, -2, -5, -40, un=6, th=0.5)]), ("Tenor Line", [V(STEP, -3, -5, 40, un=6, th=-0.5)]), ("Soprano Descant", [V(STEP, 4, -7, 30, un=6, th=1)]),
                              ("Call And Response", [V(STEP, 2, -4, -70, un=8), V(STEP, 4, -5, 70, un=8)]), ("Hymn Four Part", [V(STEP, 2, -5, -50, un=4), V(STEP, 4, -6, 50, un=4), V(STEP, -3, -6, -20, un=4), V(INT, -12, -10, 20, un=4)]),
                              ("Praise Break", [V(STEP, 2, -5, -60, un=10, pv=12), V(STEP, 4, -5, 60, un=10, pv=12)]), ("Quiet Devotion", [V(STEP, 2, -8, -40, un=4, vd=10), V(STEP, -3, -9, 40, un=4, vd=10)]),
                              ("Big Finish", [V(STEP, 2, -5, -70, un=16, sp=80), V(STEP, 4, -5, 70, un=16, sp=80), V(INT, 12, -9, 0, un=8)]), ("Church Organ Stack", [V(INT, 7, -7, -40, un=4), V(INT, 12, -8, 40, un=4)]),
                              ("Soul Sisters", [V(STEP, 2, -5, -50, un=3, th=1), V(STEP, 4, -6, 50, un=3, th=1)]), ("Choir Loft Wide", [V(STEP, 2, -6, -90, un=14, sp=100), V(STEP, -3, -6, 90, un=14, sp=100)]),
                              ("Revival", [V(STEP, 2, -5, -60, un=8, vd=20), V(STEP, 4, -6, 60, un=8, vd=20), V(STEP, -3, -7, 0, un=8, vd=20)])]):
    HC("Gospel | " + nm, vs, seed=10 + i)
# Barbershop (10)
for i, (nm, vs) in enumerate([("Classic Quartet", [V(STEP, -2, -4, -40, th=-0.5), V(STEP, 2, -5, 40), V(STEP, -5, -6, 0, th=-1)]), ("Tag Ending", [V(STEP, 2, -4, -50, vd=25), V(STEP, 4, -5, 50, vd=25), V(STEP, -3, -6, 0, vd=25)]),
                              ("Bass Anchor", [V(STEP, -7, -4, 0, th=-2), V(STEP, -3, -6, -40)]), ("Tenor Ring", [V(STEP, 2, -5, -30, th=1), V(STEP, 4, -6, 30, th=1)]), ("Close Harmony", [V(STEP, 1, -6, -30), V(STEP, 2, -6, 30), V(STEP, -1, -7, 0)]),
                              ("Doo-Wop Backing", [V(STEP, 2, -6, -60, un=2), V(STEP, -3, -6, 60, un=2), V(STEP, -7, -8, 0, un=2, th=-1)]), ("Vintage Quartet Mono", [V(STEP, -2, -4, 0), V(STEP, 2, -5, 0), V(STEP, -5, -6, 0)]),
                              ("Swing Harmony", [V(STEP, 2, -5, -50, vd=20), V(STEP, -2, -5, 50, vd=20)]), ("Seventh Chord", [V(STEP, 2, -5, -50), V(STEP, 4, -6, 50), V(STEP, 6, -8, 0)]),
                              ("Parlour Song", [V(STEP, 2, -5, -40, vd=15), V(STEP, -3, -6, 40, vd=15)])]):
    HC("Barbershop | " + nm, vs, hp=3, ht=8, seed=30 + i)
# Choir (16)
for i, (nm, un, vs) in enumerate([("Chamber Choir", 6, [V(STEP, 2, -6, -50), V(STEP, 4, -7, 50)]), ("Cathedral Choir", 16, [V(STEP, 2, -6, -60), V(STEP, -3, -6, 60), V(INT, 12, -9, 0)]),
                                  ("Unison Choir", 24, [V(INT, 0, -4, 0, sp=100)]), ("Octave Choir", 16, [V(INT, 0, -5, 0, sp=100), V(INT, 12, -8, 0, sp=100)]),
                                  ("Mens Choir", 16, [V(INT, -12, -5, 0, th=-1.5, sp=100), V(STEP, -3, -6, 0, th=-1, sp=100)]), ("Womens Choir", 16, [V(INT, 12, -7, 0, th=1.5, sp=100), V(STEP, 4, -7, 0, th=1, sp=100)]),
                                  ("Childrens Choir", 12, [V(INT, 12, -6, 0, th=3, sp=90)]), ("Thirty-Two Voice Wall", 32, [V(INT, 0, -5, 0, sp=100)]), ("Small Ensemble", 3, [V(STEP, 2, -5, -50), V(STEP, -3, -5, 50)]),
                                  ("Monastery", 12, [V(INT, -12, -5, 0, th=-2, sp=80, vd=0), V(INT, -5, -7, 0, th=-1, sp=80)]), ("Choir Pad", 20, [V(STEP, 2, -7, -80, vd=10), V(STEP, 4, -7, 80, vd=10), V(INT, -12, -10, 0)]),
                                  ("Ghost Choir", 10, [V(INT, 12, -9, -60, th=2, tb=80), V(INT, 7, -9, 60, th=2, tb=80)]), ("Stadium Singalong", 32, [V(INT, 0, -6, 0, pv=18, tv=40, sp=100)]),
                                  ("Village Chorus", 8, [V(INT, 0, -6, 0, pv=15, tv=35), V(STEP, 2, -8, -40, pv=15, tv=35)]), ("Requiem", 18, [V(STEP, -2, -6, -50, th=-1), V(STEP, 2, -7, 50), V(INT, -12, -9, 0, th=-2)]),
                                  ("Soft Choir Bed", 14, [V(STEP, 2, -9, -70, vd=12, tb=50), V(STEP, -3, -9, 70, vd=12, tb=50)])]):
    vs2 = [tuple(v[:5]) + (un,) + tuple(v[6:]) for v in vs]
    HC("Choir | " + nm, vs2, seed=50 + i, lead=-2)
# Octaves (12)
for i, (nm, vs) in enumerate([("Octave Up", [V(INT, 12, -6, 0)]), ("Octave Down", [V(INT, -12, -5, 0)]), ("Both Octaves", [V(INT, 12, -7, -40), V(INT, -12, -6, 40)]),
                              ("Two Octaves Up Sparkle", [V(INT, 24, -12, 0, th=2)]), ("Deep Octave Doubler", [V(INT, -12, -4, 0, un=3, th=-1)]), ("Wide Octave Up", [V(INT, 12, -7, -80, un=2), V(INT, 12, -7, 80, un=2)]),
                              ("Octave Down Thickener", [V(INT, -12, -7, -30, un=4), V(INT, -12, -7, 30, un=4)]), ("Octave Choir Up", [V(INT, 12, -7, 0, un=8, sp=90)]),
                              ("Octave Plus Fifth", [V(INT, 12, -7, -40), V(INT, 7, -8, 40)]), ("Sub Octave Monster", [V(INT, -12, -3, 0, th=-3)]), ("Airy Octave Up", [V(INT, 12, -9, 0, th=2, tb=60)]),
                              ("Rap Octave Hype", [V(INT, -12, -6, -40, un=2), V(INT, 12, -9, 40, un=2)])]):
    HC("Octaves | " + nm, vs, seed=70 + i)
# Thirds & Sixths (14)
for i, (nm, vs) in enumerate([("Major Third Up", [V(INT, 4, -5, -30)]), ("Minor Third Up", [V(INT, 3, -5, -30)]), ("Major Sixth Up", [V(INT, 9, -6, -30)]), ("Minor Sixth Up", [V(INT, 8, -6, -30)]),
                              ("Major Third Down", [V(INT, -4, -5, 30)]), ("Minor Third Down", [V(INT, -3, -5, 30)]), ("Sixth Below", [V(STEP, -5, -5, 30)]), ("Diatonic Third Up", [V(STEP, 2, -5, -30)]),
                              ("Diatonic Sixth Up", [V(STEP, 5, -6, -30)]), ("Thirds In Parallel", [V(STEP, 2, -5, -50), V(STEP, -2, -5, 50)]), ("Sixths In Parallel", [V(STEP, 5, -6, -50), V(STEP, -5, -6, 50)]),
                              ("Third Plus Sixth", [V(STEP, 2, -5, -50), V(STEP, 5, -7, 50)]), ("Thirds With Choir", [V(STEP, 2, -6, -60, un=6), V(STEP, -2, -6, 60, un=6)]), ("Tenth Above", [V(STEP, 9, -8, 0)])]):
    HC("Thirds & Sixths | " + nm, vs, seed=90 + i)
# Power Fifths (10)
for i, (nm, vs) in enumerate([("Fifth Up", [V(INT, 7, -5, 0)]), ("Fifth Down", [V(INT, -7, -5, 0)]), ("Fourth Up", [V(INT, 5, -5, 0)]), ("Power Stack", [V(INT, 7, -5, -40), V(INT, -5, -6, 40), V(INT, 12, -9, 0)]),
                              ("Rock Chant", [V(INT, 7, -5, -50, un=4), V(INT, -12, -6, 50, un=4)]), ("Medieval Open Fifths", [V(INT, 7, -6, -40, th=-0.5), V(INT, -5, -6, 40)]),
                              ("Anthem Fifths", [V(INT, 7, -6, -60, un=8), V(INT, 12, -8, 60, un=8)]), ("Metal Shout Stack", [V(INT, 7, -4, -40, un=3, tb=60), V(INT, -12, -5, 40, un=3)]),
                              ("Fifths And Octaves", [V(INT, 7, -6, -50), V(INT, 12, -7, 50), V(INT, -12, -8, 0)]), ("Folk Drone Fifth", [V(INT, -12, -6, -20), V(INT, -5, -7, 20)])]):
    HC("Power Fifths | " + nm, vs, seed=110 + i)
# Cinematic (14)
for i, (nm, vs) in enumerate([("Epic Wall", [V(STEP, 2, -6, -80, un=24, sp=100), V(STEP, 4, -6, 80, un=24, sp=100), V(INT, -12, -8, 0, un=16)]), ("Heavenly Host", [V(INT, 12, -8, -60, un=16, th=1.5), V(INT, 19, -11, 60, un=16, th=2)]),
                              ("Dark Ritual", [V(INT, -12, -4, 0, un=12, th=-3), V(INT, -5, -6, 0, un=12, th=-2)]), ("Trailer Swell", [V(STEP, 2, -6, -70, un=20, vd=15), V(STEP, 4, -6, 70, un=20, vd=15), V(INT, 12, -9, 0, un=20)]),
                              ("Sacred Space", [V(INT, 7, -8, -50, un=14), V(INT, 12, -9, 50, un=14)]), ("Ancient Chant", [V(INT, -12, -5, 0, un=10, th=-2), V(INT, 7, -8, 0, un=10)]),
                              ("Angel Halo", [V(INT, 12, -9, -80, un=10, th=2, tb=70), V(INT, 24, -14, 80, un=10, th=3)]), ("Battle Hymn", [V(STEP, 2, -5, -60, un=28, pv=15), V(STEP, -3, -5, 60, un=28, pv=15)]),
                              ("Ghost Ship", [V(INT, -5, -8, -60, un=8, tb=80, th=1), V(INT, 7, -9, 60, un=8, tb=80, th=1)]), ("Galactic Choir", [V(INT, 7, -8, -70, un=16), V(INT, 14, -10, 70, un=16)]),
                              ("Requiem Lament", [V(STEP, -2, -6, -50, un=12, th=-1, vd=12), V(STEP, -5, -7, 50, un=12, th=-1, vd=12)]), ("Hero Theme", [V(STEP, 2, -5, -60, un=16), V(STEP, 4, -6, 60, un=16), V(INT, -12, -7, 0, un=16)]),
                              ("Mystery Cluster", [V(INT, 1, -9, -60, un=6), V(INT, 2, -9, 60, un=6), V(INT, -1, -10, 0, un=6)]), ("Final Credits", [V(STEP, 2, -6, -80, un=12, vd=10), V(STEP, 4, -7, 80, un=12, vd=10)])]):
    HC("Cinematic | " + nm, vs, seed=130 + i, lead=-1)
# Robot Chorus (14) - MIDI driven and hard
for i, (nm, vs, hp) in enumerate([("MIDI Chord Stack", [V(CHORD, 0, -5, -60), V(CHORD, 0, -5, 60), V(CHORD, 0, -6, 0)], 0), ("MIDI Omni Choir", [V(OMNI, 0, -5, -60, un=6), V(OMNI, 0, -5, 60, un=6), V(OMNI, 0, -6, 0, un=6)], 2),
                                  ("Robot Unison Wall", [V(INT, 0, -4, 0, un=16, pv=0, tv=2, tb=0, sp=100)], 0), ("Disco Robot Chorus", [V(INT, 12, -6, -60, pv=0, tv=0), V(INT, -12, -6, 60, pv=0, tv=0)], 0),
                                  ("Vocoder-ish Stack", [V(INT, 7, -6, -50, th=-3, pv=0), V(INT, 12, -7, 50, th=-3, pv=0), V(INT, -12, -6, 0, th=-3, pv=0)], 0), ("Cyber Fifths", [V(INT, 7, -5, -70, pv=0, un=4, tv=1), V(INT, -5, -5, 70, pv=0, un=4, tv=1)], 0),
                                  ("Chipmunk Choir", [V(INT, 12, -7, -50, th=6, un=6), V(INT, 19, -9, 50, th=6, un=6)], 3), ("Giant Bots", [V(INT, -12, -4, 0, th=-6, pv=0)], 0),
                                  ("MIDI Pad Voices", [V(CHORD, 0, -6, -70, un=8, vd=10), V(CHORD, 0, -6, 70, un=8, vd=10), V(CHORD, 0, -7, 0, un=8)], 4), ("Glitch Cluster", [V(INT, 1, -8, -70, pv=0), V(INT, -1, -8, 70, pv=0), V(INT, 6, -9, 0, pv=0)], 0),
                                  ("Hard Tune Harmony", [V(STEP, 2, -5, -50, pv=0, tv=0), V(STEP, 4, -6, 50, pv=0, tv=0)], 0), ("Alien Choir", [V(INT, 6, -7, -60, th=4, un=8), V(INT, -6, -7, 60, th=-4, un=8)], 2),
                                  ("Arena Robots", [V(INT, 0, -5, 0, un=24, pv=0, tv=5, sp=100), V(INT, 12, -9, 0, un=8, pv=0)], 0), ("Talkbox Trio", [V(INT, 4, -5, -40, th=-2, pv=0), V(INT, 7, -6, 40, th=-2, pv=0)], 0)]):
    HC("Robot Chorus | " + nm, vs, hp=hp, ht=0 if hp == 0 else 6, cons=15, seed=150 + i)

#=====================================================================================================================
# Double Wing 2
DV = ['pitchVar', 'timeVar', 'timbre', 'spread', 'vibRate', 'vibDepth', 'doublesLevel', 'lead', 'width', 'output']
DD = dict(pitchVar=9, timeVar=22, timbre=30, spread=80, vibRate=5.4, vibDepth=0, doublesLevel=-3, lead=0, width=100, output=0)
DP = ['doubles', 'voiceRange', 'vibStraightOn', 'seed']
plug('Double Wing 2', DV, DP, DD)
def DW(name, n=2, pv=9, tv=22, tb=30, sp=80, vr=5.4, vd=0, dl=-3, lead=0, width=100, vrange=1, straight=1, seed=3):
    add('Double Wing 2', name, dict(pitchVar=pv, timeVar=tv, timbre=tb, spread=sp, vibRate=vr, vibDepth=vd, doublesLevel=dl, lead=lead, width=width),
        dict(doubles=n, voiceRange=vrange, vibStraightOn=straight, seed=seed))
for i, (nm, n, pv, tv, tb, sp, dl) in enumerate([("Tight Pop Double", 1, 5, 12, 15, 0, -4), ("Tight Pair", 2, 6, 14, 20, 60, -5), ("Natural Double", 1, 9, 22, 30, 0, -3), ("Natural Wide Pair", 2, 9, 22, 30, 85, -4),
                                                  ("Loose Double", 1, 15, 38, 40, 0, -3), ("Loose Wide Pair", 2, 15, 40, 40, 90, -4), ("Quad Stack", 4, 10, 28, 35, 90, -6), ("Triple Stack", 3, 9, 24, 30, 80, -5),
                                                  ("Hook Gang", 4, 14, 35, 45, 100, -5), ("Whisper Double", 2, 7, 18, 50, 70, -8)]):
    DW("Vocal Doubles | " + nm, n, pv, tv, tb, sp, dl=dl, seed=1 + i)
for i, (nm, n, pv, tv, sp, dl) in enumerate([("Rap Double Center", 1, 6, 10, 0, -4), ("Rap Ad-Lib Wide", 2, 8, 16, 100, -5), ("Trap Stack", 3, 7, 14, 90, -5), ("Drill Double", 2, 5, 11, 70, -5),
                                             ("Boom Bap Pair", 2, 10, 24, 80, -5), ("Hype Man", 3, 14, 30, 100, -4), ("Gang Vocal", 4, 16, 42, 100, -5), ("Low Rap Shadow", 1, 4, 8, 0, -7)]):
    DW("Rap Doubles | " + nm, n, pv, tv, 20, sp, dl=dl, seed=20 + i)
for i, (nm, n, pv, tv, tb, sp, dl, w) in enumerate([("Tape Double Track", 1, 4, 30, 10, 0, -3, 100), ("Classic ADT Pair", 2, 5, 25, 15, 100, -4, 120), ("Beat Era Double", 1, 7, 45, 20, 0, -3, 100),
                                                     ("Psych Double", 2, 12, 55, 40, 100, -4, 150), ("Wide ADT", 2, 6, 28, 15, 100, -5, 160), ("Slap Double", 1, 3, 70, 10, 30, -5, 100),
                                                     ("Soul Double", 2, 8, 24, 30, 70, -4, 100), ("Fifties Double", 1, 5, 35, 20, 0, -4, 100)]):
    DW("Vintage ADT | " + nm, n, pv, tv, tb, sp, dl=dl, width=w, seed=40 + i)
for i, (nm, n, vd, vr, pv, sp) in enumerate([("Straight Note Shimmer", 2, 25, 5.4, 6, 80), ("Ballad Vibrato Doubles", 2, 35, 5.0, 8, 70), ("Soft Vibrato Halo", 3, 20, 5.8, 6, 90),
                                              ("Operatic Doubles", 2, 50, 5.6, 7, 70), ("Choir Vibrato", 4, 30, 5.2, 12, 100), ("Gentle Wobble", 1, 15, 4.5, 5, 0),
                                              ("Pop Belt Vibrato", 2, 40, 6.0, 7, 80), ("Wide Vibrato Pad", 4, 25, 5.0, 10, 100)]):
    DW("Vibrato Doubles | " + nm, n, pv, 22, 30, sp, vr=vr, vd=vd, dl=-5, seed=60 + i)
for i, (nm, n, pv, tv, tb, sp, dl, w) in enumerate([("Lo-Fi Wobble Double", 2, 22, 30, 60, 80, -5, 100), ("Drunk Choir", 4, 30, 60, 60, 100, -6, 150), ("Chorus Double", 2, 18, 8, 30, 100, -4, 120),
                                                     ("Huge Wide Stack", 4, 12, 35, 50, 100, -6, 200), ("Mono Thickener", 2, 8, 15, 30, 0, -4, 0), ("Ghost Echo Double", 1, 10, 80, 70, 60, -9, 100),
                                                     ("Seasick Pair", 2, 35, 30, 40, 90, -5, 120), ("Bright Air Doubles", 2, 8, 20, 90, 80, -7, 120), ("Dark Shadow Doubles", 2, 8, 26, 90, 80, -7, 120),
                                                     ("Arena Gang", 4, 18, 50, 50, 100, -5, 160)]):
    DW("Character | " + nm, n, pv, tv, tb, sp, dl=dl, width=w, seed=80 + i)
for i, (nm, n, pv, tv, vrange) in enumerate([("Soprano Doubles", 2, 7, 20, 0), ("Tenor Doubles", 2, 9, 22, 1), ("Baritone Doubles", 2, 9, 24, 2), ("Bass Voice Doubles", 2, 8, 26, 2),
                                              ("Guitar Doubler", 2, 6, 18, 3), ("Synth Lead Doubler", 2, 5, 15, 3), ("Bass Doubler Mono", 1, 4, 10, 4), ("Instrument Wide Pair", 2, 8, 22, 3),
                                              ("Strings Section", 4, 12, 30, 3), ("Brass Section", 3, 9, 24, 3), ("Keys Widener", 2, 6, 16, 3), ("Female Lead Pair", 2, 8, 20, 0),
                                              ("Male Lead Pair", 2, 8, 22, 2), ("Child Voice Pair", 2, 10, 20, 0), ("Wide Range Auto", 2, 9, 22, 5), ("Narrator Double", 1, 4, 12, 2)]):
    DW("By Source | " + nm, n, pv, tv, 30, 60 if n == 1 and 'Mono' in nm else 80, dl=-5, vrange=vrange, seed=100 + i)

#=====================================================================================================================
# Mutant
MV = ['pitch', 'throat', 'ringMix', 'ringRatio', 'ringFallback', 'alien', 'alienGlide', 'stutter', 'drive', 'mix', 'output']
MD = dict(pitch=0, throat=0, ringMix=0, ringRatio=1, ringFallback=220, alien=0, alienGlide=30, stutter=0, drive=30, mix=100, output=0)
MP = ['engine', 'voiceType', 'alienDiv', 'stutterDiv', 'repeats', 'driveVoicing', 'seed']
plug('Mutant', MV, MP, MD)
# divisions: 6 1/16, 9 1/8, 10 1/8., 11 1/8T, 12 1/4, 15 1/2
def MU(name, pitch=0, throat=0, ring=0, ratio=1, fb=220, alien=0, adiv=9, glide=30, stutter=0, sdiv=6, rep=2, drv=0, drive=30, mix=100, eng=0, seed=5):
    add('Mutant', name, dict(pitch=pitch, throat=throat, ringMix=ring, ringRatio=ratio, ringFallback=fb, alien=alien, alienGlide=glide, stutter=stutter, drive=drive, mix=mix),
        dict(engine=eng, voiceType=1, alienDiv=adiv, stutterDiv=sdiv, repeats=rep, driveVoicing=drv, seed=seed))
# Monsters (10)
for i, (nm, p, t, drv, d) in enumerate([("Demon Lord", -12, -8, 6, 50), ("Ogre", -9, -10, 2, 40), ("Abyss Beast", -17, -12, 7, 60), ("Swamp Thing", -7, -9, 4, 50), ("Minotaur", -10, -6, 5, 55),
                                        ("Cave Troll", -14, -14, 2, 45), ("Hellhound", -5, -7, 6, 70), ("Kraken", -19, -16, 3, 40), ("Golem", -12, -4, 5, 40), ("Nightmare", -8, -11, 7, 65)]):
    MU("Monsters | " + nm, p, t, drv=drv, drive=d, eng=1, seed=10 + i)
# Aliens (10)
for i, (nm, p, t, al, adiv, gl) in enumerate([("Martian Chatter", 4, 6, 50, 6, 20), ("Grey Visitor", 7, 9, 30, 9, 40), ("Hive Mind", 0, 5, 60, 6, 10), ("Starship AI", 2, -3, 40, 12, 80),
                                               ("Insectoid", 12, 12, 45, 6, 5), ("Space Oracle", -5, 4, 35, 15, 200), ("Glitch Envoy", 3, 8, 80, 6, 0), ("Nebula Whisper", 9, 10, 25, 12, 150),
                                               ("Cosmic Trickster", 0, 0, 70, 9, 15), ("Tentacle Diplomat", -3, 7, 55, 10, 60)]):
    MU("Aliens | " + nm, p, t, alien=al, adiv=adiv, glide=gl, seed=30 + i)
# Robots (10)
for i, (nm, p, t, ring, ratio, drv) in enumerate([("Classic Robot", 0, -2, 70, 1, 0), ("Tin Sentinel", -5, -3, 80, 0.5, 4), ("Android Host", 0, 2, 50, 2, 0), ("Factory Unit", -7, -5, 90, 1, 5),
                                                  ("Laser Bot", 5, 4, 70, 3, 0), ("Old Computer", 0, 0, 100, 1, 2), ("Mecha Commander", -12, -4, 60, 0.5, 6), ("Toy Robot", 7, 8, 80, 2, 0),
                                                  ("Broken Droid", -3, 0, 90, 1.5, 7), ("Ring Overlord", -9, -6, 100, 0.25, 5)]):
    MU("Robots | " + nm, p, t, ring=ring, ratio=ratio, drv=drv, drive=40, seed=50 + i)
# Ring Mod (8)
for i, (nm, ring, ratio, fb, p) in enumerate([("Bell Voice", 60, 2.5, 440, 0), ("Metal Throat", 80, 0.7, 150, 0), ("Sideband Shimmer", 40, 3, 800, 0), ("Subharmonic Growl", 70, 0.5, 110, -5),
                                              ("Fifth Ring", 60, 1.5, 330, 0), ("Detuned Ring", 50, 1.03, 220, 0), ("Telephone Ghost", 50, 4, 1200, 3), ("Octave Ring", 70, 2, 440, 0)]):
    MU("Ring Mod | " + nm, p, ring=ring, ratio=ratio, fb=fb, seed=70 + i)
# Glitch & Stutter (12)
for i, (nm, st, sdiv, rep, al, adiv) in enumerate([("Sixteenth Stutter", 40, 6, 2, 0, 9), ("Eighth Stutter", 40, 9, 2, 0, 9), ("Machine Gun", 60, 3, 5, 0, 9), ("Glitch Vocal", 50, 6, 2, 40, 6),
                                                   ("Dotted Chop", 45, 10, 1, 0, 9), ("Triplet Glitch", 50, 11, 1, 30, 11), ("Broken Tape", 35, 9, 3, 50, 9), ("Stutter Build", 70, 6, 4, 0, 9),
                                                   ("Granular Mess", 60, 3, 3, 60, 6), ("Rhythm Scramble", 50, 9, 2, 70, 9), ("Quarter Repeat", 40, 12, 1, 0, 9), ("Hard Cut Ups", 55, 6, 1, 20, 6)]):
    MU("Glitch & Stutter | " + nm, stutter=st, sdiv=sdiv, rep=rep, alien=al, adiv=adiv, glide=5, seed=90 + i)
# Creatures & Characters (10)
for i, (nm, p, t, ring, ratio, al) in enumerate([("Chipmunk Choirboy", 12, 6, 0, 1, 0), ("Pixie", 9, 10, 0, 1, 15), ("Tiny Gremlin", 7, 8, 20, 2, 25), ("Giant Baby", -7, 9, 0, 1, 0),
                                                 ("Ghost", 0, 4, 30, 0.5, 20), ("Witch", 3, 7, 15, 1.5, 30), ("Dragon", -12, -9, 25, 0.5, 10), ("Sea Siren", 5, 3, 20, 2, 15),
                                                 ("Puppet", 5, 10, 0, 1, 0), ("Shapeshifter", 0, 0, 30, 1.5, 90)]):
    MU("Creatures | " + nm, p, t, ring=ring, ratio=ratio, alien=al, adiv=12, glide=120, drv=0, seed=110 + i)

#=====================================================================================================================
WANT = {'Hive Choir': 120, 'Double Wing 2': 60, 'Mutant': 60}
CPP = {'Hive Choir': 'choir', 'Double Wing 2': 'dbl', 'Mutant': 'mut'}

def write_table(path):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111h.py - do not edit.\n#pragma once\n')
        f.write('struct E2Row { const char* name; float v[64]; int pr[16]; };\n')
        for nm, (params, props, lst, defs) in PLUGS.items():
            f.write('// %s values: %s | props: %s\n' % (nm, ' '.join(params), ' '.join(props)))
            f.write('static const E2Row %sRows[] = {\n' % CPP[nm])
            for p in lst:
                v = [p['vals'].get(k, defs[k]) for k in params]
                pr = [int(p['props'].get(k, 0)) for k in props]
                f.write('    { %s, { %s }, { %s } },\n' % (json.dumps(p['name']), ', '.join('%g' % x for x in v), ', '.join(str(x) for x in pr)))
            f.write('};\n')

def calibrate():
    exe = subprocess.check_output(['bash', os.path.join(ROOT, 'scripts', 'build_voice111e_test.sh'), os.path.join(ROOT, 'tests', 'preset_calib_e2.cpp'), 'preset_calib_e2'], text=True).strip().splitlines()[-1]
    out = subprocess.run([exe], text=True, capture_output=True, check=True)
    open('/tmp/calib_e2.log', 'w').write(out.stderr)
    trims = {}
    for line in out.stdout.splitlines():
        if line.startswith('TRIM\t'):
            _, plug_, name, trim, peak = line.split('\t')
            trims[(plug_, name)] = (float(trim), float(peak))
    return trims

def num(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def main():
    for nm, (params, props, lst, defs) in PLUGS.items():
        names = [p['name'] for p in lst]
        dup = set(n for n in names if names.count(n) > 1)
        assert not dup, (nm, dup)
        print('%s: %d presets (want %d) in %d categories' % (nm, len(lst), WANT[nm], len(set(n.split(' | ')[0] for n in names))))
        assert len(lst) == WANT[nm], nm
    write_table(os.path.join(ROOT, 'tests', 'presets111h_table.h'))
    trims = calibrate()
    fns, adds = [], []; worst = 0
    for k, (nm, (params, props, lst, defs)) in enumerate(PLUGS.items()):
        lines = []
        for p in lst:
            trim, peak = trims[(nm, p['name'])]
            trim = max(-24.0, min(12.0, min(trim, 3.0 - peak))); worst = max(worst, abs(trim))
            vals = ', '.join('N (%s, %s)' % (json.dumps(kk), num(vv)) for kk, vv in p['vals'].items()) + ', N ("output", %s)' % num(round(trim, 2))
            prs = ', '.join('{ "%s", %d }' % (kk, int(vv)) for kk, vv in p['props'].items())
            lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111h_%d()   // %s\n{\n    return {\n%s\n    };\n}\n' % (k + 1, nm, '\n'.join(lines)))
        adds.append('    m[%s] = presets111h_%d();\n' % (json.dumps(nm), k + 1))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111h.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111h.py - factory presets for the 1.11 Voice second wave (part 2). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111g.inl; bank() calls add111h().\n\n')
        f.write('\n'.join(fns))
        f.write('inline void add111h (std::map<juce::String, std::vector<Preset>>& m)\n{\n' + ''.join(adds) + '}\n')
    print('wrote', out, '- largest trim %.2f dB' % worst)

if __name__ == '__main__':
    main()
