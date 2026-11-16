#!/usr/bin/env python3
"""Factory presets for the 1.11 Color & Space second wave, part 2 (super prompt C3, C5, C15):
   Shard (220), Murmuration (120), Masquerade (100).

Every preset is rendered through the real DSP (tests/preset_calib_d2.cpp, built from Source/SqColor111cDsp.h and
Source/SqMasqueradeDsp.h against JUCE core) and its Output trim is written from the measurement so it holds its level on
the material it was made for (vocal presets and every Masquerade preset on the sung vocal, drum presets on the drum loop,
everything else on pink noise), with the peak kept under +3 dBFS.

Writes tests/presets111e_table.h and Source/SignaturePresets111e.inl (add111e(), called from SignaturePresets::bank()).
"""
import json, os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D32, D16, D16D, D16T, D8, D8D, D8T, D4, D4D, D4T, D2, D2D, BAR, BAR2, BAR4 = 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 21, 24
SINE, TRI, RUP, RDN, SQR, PUL, SH, SRND, CUST = range(9)
PLUGS = {}
def plug(name, cls, props): PLUGS[name] = (cls, props, [])
def add(name, preset, vals, props):
    cls, propids, lst = PLUGS[name]
    for k in props: assert k in propids, (name, k)
    lst.append(dict(name=preset, vals=vals, props=props))

#=====================================================================================================================
# Shard. sync props: 0 free, else WingMod division + 1. targets: 0 off 1 pitch 2 splice 3 delay 4 recycle 5 spray 6 mix 7 reverse
plug('Shard', 'SqShardPlugin', ['engine', 'fbMode', 'dynMode', 'keyOn', 'quantizeOn', 'autoKeyOn', 'keyRoot', 'scale', 'spliceSync', 'delaySync',
                                'lfo1Target', 'lfo1Shape', 'lfo1Sync', 'lfo2Target', 'lfo2Shape', 'lfo2Sync', 'envTarget'])
VIN, CLN = 0, 1
MIXED, DUAL, PING = 0, 1, 2
T_P, T_S, T_D, T_R, T_SP, T_M, T_REV = 1, 2, 3, 4, 5, 6, 7
def S(name, pitch, splice=250, delay=0, rec=30, mix=40, rev=100, eng=VIN, smooth=25, spray=0, dens=1, jit=0, pl=0, pr=0, sor=0, dor=0, lc=1, hc=20000,
      fb=MIXED, dyn=0, dthr=-30, drel=200, quant=0, ssync=0, dsync=0, l1=(0, SINE, 0, 0, 0.5), l2=(0, TRI, 0, 0, 2.0), env=(0, 0, 10, 250)):
    vals = dict(splice=splice, delay=delay, pitch=pitch, recycle=rec, mix=mix, reverse=rev, smoothing=smooth, spray=spray, density=dens, sizeJitter=jit,
                pitchL=pl, pitchR=pr, spliceOffR=sor, delayOffR=dor, lowCut=lc, highCut=hc, dynThresh=dthr, dynRelease=drel,
                lfo1Depth=l1[3], lfo1Rate=l1[4], lfo2Depth=l2[3], lfo2Rate=l2[4], envDepth=env[1], envAttack=env[2], envRelease=env[3])
    props = dict(engine=eng, fbMode=fb, dynMode=dyn, keyOn=0, quantizeOn=quant, autoKeyOn=1, keyRoot=0, scale=1, spliceSync=ssync, delaySync=dsync,
                 lfo1Target=l1[0], lfo1Shape=l1[1], lfo1Sync=l1[2], lfo2Target=l2[0], lfo2Shape=l2[1], lfo2Sync=l2[2], envTarget=env[0])
    add('Shard', name, vals, props)
# Crystals (30): the classic reverse-octave shimmer and its relatives
for i, (nm, p, sp, rec, mix) in enumerate([
    ("Classic Crystal", 12, 250, 35, 40), ("Glass Octave", 12, 180, 30, 35), ("Ice Cathedral", 12, 420, 55, 40), ("Frozen Lake", 12, 600, 50, 35),
    ("Shattered Mirror", 12, 120, 40, 40), ("Prism Rain", 24, 200, 35, 35), ("Diamond Dust", 24, 90, 30, 30), ("Snow Globe", 12, 320, 45, 38),
    ("Crystal Ladder", 12, 160, 60, 35), ("Starlight Reverse", 24, 360, 45, 32), ("Quartz Bloom", 12, 520, 40, 40), ("Moonstone", 7, 300, 40, 35),
    ("Sapphire", 5, 260, 35, 35), ("Opal Haze", 12, 700, 55, 30), ("Glacier", -12, 450, 45, 38), ("Icicle Drip", 24, 70, 25, 30),
    ("Chime Crystal", 19, 150, 35, 32), ("Clear Glass", 12, 240, 25, 30), ("Mineral", 7, 140, 30, 35), ("Frost Echo", 12, 280, 50, 36),
    ("Northern Lights", 12, 800, 60, 32), ("Crystal Choir", 12, 380, 55, 40), ("Silver Bell", 24, 130, 40, 30), ("Polar", -12, 330, 40, 36),
    ("Spun Sugar", 12, 100, 30, 34), ("Halo Shard", 12, 460, 45, 36), ("Faceted", 12, 210, 35, 38), ("Gem Cascade", 12, 170, 65, 32),
    ("Bright Splinter", 24, 110, 30, 30), ("Deep Crystal", -12, 560, 50, 38)]):
    S("Crystals | " + nm, p, sp, delay=[0, 40, 80, 120][i % 4], rec=rec, mix=mix, eng=CLN if i % 3 == 2 else VIN, pl=[0, -6, 0, 4][i % 4], pr=[0, 6, 0, -4][i % 4])
# Reverse (24): pitch-neutral and small intervals, all backwards
for i, nm in enumerate(["Backwards Tape", "Reverse Slap", "Reverse Swell", "Tape Rewind", "Mirror Time", "Inside Out", "Suck Back", "Backward Bloom",
                        "Reverse Room", "Reverse Halo", "Flip Echo", "Rewound Vocal", "Long Reverse", "Short Reverse", "Reverse Ping", "Reverse Dual",
                        "Reverse Wash", "Reverse Lift", "Reverse Drop", "Reverse Shimmer", "Half Reverse", "Random Reverse", "Synced Reverse", "Reverse Haze"]):
    p = [0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 7, -5, 12, 0, 0, 0, -12][i]
    S("Reverse | " + nm, p, [400, 150, 700, 900, 300, 500, 250, 600, 350, 450, 200, 380, 1500, 90, 300, 320, 1200, 400, 400, 500, 300, 280, 0, 800][i],
      delay=[0, 60, 200, 0, 100, 0, 0, 150, 50, 80, 120, 40, 0, 30, 0, 0, 300, 0, 0, 100, 0, 0, 0, 200][i], rec=[25, 15, 40, 30, 35, 30, 20, 45, 40, 45, 30, 25, 35, 15, 35, 35, 55, 30, 30, 50, 30, 30, 30, 50][i],
      mix=[40, 35, 45, 42, 40, 40, 38, 40, 35, 35, 38, 34, 40, 34, 36, 38, 30, 38, 38, 34, 36, 36, 38, 32][i],
      rev=[50, 60, 100, 100][i - 20] if i >= 20 else 100, fb=PING if 'Ping' in nm else DUAL if 'Dual' in nm else MIXED,
      ssync=(D4 + 1) if 'Synced' in nm else 0, spray=30 if 'Haze' in nm or 'Wash' in nm else 0, sor=-20 if 'Dual' in nm else 0, dor=60 if 'Dual' in nm else 0)
# Octaves (24)
for i, (nm, p) in enumerate([("Octave Up", 12), ("Octave Down", -12), ("Two Up", 24), ("Two Down", -24), ("Up And Down", 12), ("Sub Octave Shadow", -12),
                             ("Octave Ladder", 12), ("Octave Forward", 12), ("Octave Down Forward", -12), ("Clean Octave", 12), ("Clean Sub", -12), ("Three Up", 36),
                             ("Three Down", -36), ("Octave Spray", 12), ("Octave Dual", 12), ("Octave Ping-Pong", 12), ("Octave Mist", 12), ("Low Rumble", -24),
                             ("Octave Shimmer Long", 12), ("Octave Short", 12), ("Octave Stack", 24), ("Organ Octave", 12), ("Octave Glide", 12), ("Octave Gate", 12)]):
    S("Octaves | " + nm, p, [250, 300, 200, 400, 300, 350, 180, 220, 280, 260, 320, 150, 500, 240, 270, 250, 600, 450, 900, 90, 200, 300, 260, 220][i],
      rec=[30, 30, 35, 35, 45, 25, 60, 30, 30, 30, 30, 35, 35, 40, 35, 40, 50, 30, 55, 25, 50, 35, 40, 30][i], mix=[38, 40, 32, 40, 38, 40, 32, 38, 40, 38, 40, 30, 40, 36, 36, 36, 32, 40, 30, 36, 30, 40, 36, 38][i],
      rev=0 if 'Forward' in nm or 'Organ' in nm else 100, eng=CLN if 'Clean' in nm or 'Organ' in nm else VIN, pl=-12 if 'Up And Down' in nm else 0, pr=12 if False else 0,
      fb=PING if 'Ping' in nm else DUAL if 'Dual' in nm else MIXED, spray=40 if 'Spray' in nm or 'Mist' in nm else 0, dyn=1 if 'Gate' in nm else 0, dthr=-26,
      l1=(T_P, SINE, 0, 4, 0.3) if 'Glide' in nm else (0, SINE, 0, 0, 0.5))
# Fifths & Harmony (24): quantized intervals follow the key
for i, (nm, p) in enumerate([("Perfect Fifth", 7), ("Fourth", 5), ("Fifth Below", -5), ("Major Third", 4), ("Minor Third", 3), ("Major Sixth", 9),
                             ("Octave Fifth", 19), ("Power Chord", 7), ("Fourth Below", -7), ("Ninth", 14), ("Major Seventh", 11), ("Minor Seventh", 10),
                             ("Third In Key", 4), ("Fifth In Key", 7), ("Sixth In Key", 9), ("Second In Key", 2), ("Tenth In Key", 16), ("Harmony Cloud", 7),
                             ("Fifth Cascade", 7), ("Fourth Cascade", 5), ("Clean Fifth", 7), ("Clean Third", 4), ("Choir Fifth", 7), ("Detuned Fifth", 7)]):
    S("Fifths & Harmony | " + nm, p, [260, 300, 280, 240, 240, 300, 200, 220, 300, 260, 280, 280, 260, 260, 280, 240, 260, 400, 220, 240, 300, 300, 450, 260][i],
      rec=60 if 'Cascade' in nm else 30, mix=36, rev=50 if i % 2 else 100, quant=1 if 'In Key' in nm or 'Cloud' in nm else 0,
      eng=CLN if 'Clean' in nm or 'Choir' in nm else VIN, spray=35 if 'Cloud' in nm or 'Choir' in nm else 0, dens=2 if 'Choir' in nm else 1,
      pl=-9 if 'Detuned' in nm else 0, pr=9 if 'Detuned' in nm else 0)
# Glitch (22)
for i, nm in enumerate(["Stutter Shards", "Bit Splinters", "Random Pitch", "Scatter Gun", "Chopped Echo", "Broken Tape", "Glitch Arp", "Micro Splice",
                        "Sample And Hold", "Digital Hiccup", "Jitter Box", "Grain Storm", "Lo-Fi Splice", "Skipping CD", "Glitch Ping", "Fragment",
                        "Stuck Buffer", "Rhythmic Glitch", "Tape Spin", "Machine Gun", "Shard Dice", "Warped Shards"]):
    S("Glitch | " + nm, [0, 12, 0, 7, 0, -5, 12, 0, 0, 12, 0, 0, -12, 0, 7, 5, 0, 12, 0, 0, 0, -7][i],
      [60, 40, 120, 80, 100, 150, 120, 25, 120, 50, 90, 70, 110, 45, 100, 60, 30, 0, 200, 20, 80, 180][i], rec=[50, 40, 45, 35, 60, 40, 50, 30, 45, 35, 40, 55, 40, 70, 45, 35, 80, 50, 40, 60, 45, 50][i],
      mix=38, rev=[30, 50, 50, 70, 0, 60, 0, 50, 40, 50, 50, 50, 60, 0, 50, 70, 0, 50, 100, 0, 50, 60][i], spray=[0, 20, 40, 80, 0, 30, 0, 60, 30, 0, 70, 90, 20, 0, 40, 60, 0, 30, 0, 0, 50, 40][i],
      jit=[0, 40, 60, 80, 20, 30, 0, 70, 50, 30, 90, 80, 30, 0, 40, 60, 0, 30, 20, 0, 60, 50][i], dens=[1, 2, 1, 3, 1, 1, 2, 4, 1, 2, 2, 4, 1, 1, 1, 3, 1, 2, 1, 1, 2, 2][i],
      smooth=[5, 3, 10, 5, 8, 15, 5, 3, 5, 2, 6, 10, 8, 2, 8, 6, 2, 5, 25, 2, 6, 15][i], hc=8000 if 'Lo-Fi' in nm else 20000, fb=PING if 'Ping' in nm else MIXED,
      ssync=(D16 + 1) if 'Rhythmic' in nm else 0, l1=(T_P, SH, 0, 30, 6.0) if nm in ('Random Pitch', 'Sample And Hold', 'Shard Dice') else (T_P, SINE, 0, 10, 0.5) if 'Warped' in nm or 'Spin' in nm else (0, SINE, 0, 0, 0.5))
# Pads (22)
for i, nm in enumerate(["Endless Pad", "Choir Of Glass", "Slow Bloom", "Shimmer Pad", "Dark Pad", "Warm Pad", "Ocean Pad", "Velvet Pad", "Halo Pad", "Drifting Pad",
                        "Octave Pad", "Fifth Pad", "Sub Pad", "Air Pad", "Cloud Pad", "Long Tail Pad", "Frozen Pad", "Swell Pad", "Pad Wash", "Glass Pad", "Twilight Pad", "Dream Pad"]):
    S("Pads | " + nm, [12, 12, 0, 24, -12, 7, 12, 5, 12, 0, 12, 7, -12, 24, 12, 12, 12, 0, 12, 24, 7, 12][i], [800, 600, 1200, 700, 900, 650, 1500, 800, 1000, 1100, 750, 700, 900, 600, 800, 2000, 1800, 1300, 1600, 500, 1000, 900][i],
      rec=[75, 70, 60, 75, 65, 60, 80, 65, 70, 70, 70, 65, 60, 70, 75, 85, 90, 60, 80, 70, 70, 75][i], mix=34, rev=50, eng=CLN, spray=[50, 60, 40, 50, 40, 30, 70, 30, 50, 60, 40, 40, 30, 60, 80, 50, 40, 30, 70, 40, 50, 60][i],
      dens=[2, 3, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 3, 3, 2, 2, 2, 3, 2, 2, 3][i], smooth=45, jit=30, hc=[12000, 14000, 10000, 16000, 5000, 7000, 12000, 8000, 14000, 10000, 12000, 10000, 4000, 18000, 12000, 10000, 12000, 9000, 11000, 16000, 9000, 13000][i],
      l1=(T_P, SINE, 0, 1, 0.15), env=(T_M, 0, 10, 250) if 'Swell' not in nm else (T_M, -40, 50, 2000))
# Vocal FX (22)
for i, nm in enumerate(["Vocal Crystal", "Vocal Octave Up", "Vocal Octave Down", "Ad-Lib Reverse", "Vocal Fifth", "Hook Shimmer", "Vocal Ghost", "Chorus Halo",
                        "Rap Throw", "Vocal Glitch", "Breath Reverse", "Vocal Stack", "Ballad Glow", "Vocal Third In Key", "Spoken Word Shard", "Choir Up",
                        "Vocal Swell", "Vocal Ducked Crystal", "Vocal Gate Shards", "Vocal Low Double", "Vocal Air", "Vocal Ping"]):
    S("Vocal FX | " + nm, [12, 12, -12, 0, 7, 24, 12, 12, 0, 0, 0, 12, 12, 4, -5, 12, 12, 12, 12, -12, 24, 12][i], [250, 300, 300, 400, 260, 200, 500, 350, 220, 70, 600, 260, 450, 280, 300, 400, 700, 300, 150, 280, 180, 260][i],
      delay=[0, 50, 0, 100, 0, 30, 200, 60, 120, 0, 0, 0, 100, 0, 80, 0, 0, 0, 0, 0, 40, 0][i], rec=[30, 25, 25, 25, 30, 35, 45, 40, 20, 40, 30, 35, 40, 30, 25, 45, 55, 35, 30, 20, 30, 35][i],
      mix=[30, 30, 30, 32, 28, 26, 26, 30, 28, 30, 30, 30, 28, 30, 32, 30, 26, 40, 40, 28, 24, 30][i], rev=[100, 0, 0, 100, 50, 100, 100, 50, 100, 50, 100, 0, 100, 0, 100, 0, 100, 100, 100, 0, 100, 100][i],
      eng=CLN if i in (1, 2, 11, 13, 15, 19) else VIN, quant=1 if 'In Key' in nm else 0, dyn=2 if 'Ducked' in nm else 1 if 'Gate' in nm else 0, dthr=-28,
      spray=40 if 'Choir' in nm else 0, dens=2 if 'Choir' in nm or 'Stack' in nm else 1, fb=PING if 'Ping' in nm else MIXED, lc=150, hc=14000)
# Drums (20)
for i, nm in enumerate(["Snare Crystal", "Kick Sub Splice", "Hat Glitter", "Drum Reverse", "Break Shards", "Clap Shimmer", "Tom Octave", "Loop Glitch",
                        "Drum Fifth", "Perc Spray", "Snare Swell", "Room Shards", "Drum Ping", "Kit Stutter", "Rim Chime", "Drum Ducked", "Drum Bus Shimmer",
                        "Groove Reverse", "Sixteenth Shards", "Eighth Shards"]):
    S("Drums | " + nm, [12, -12, 24, 0, 12, 12, 12, 0, 7, 12, 0, 12, 12, 0, 24, 12, 12, 0, 12, 12][i], [150, 200, 80, 250, 120, 180, 220, 90, 200, 100, 400, 300, 150, 60, 120, 200, 250, 0, 0, 0][i],
      rec=[30, 20, 25, 25, 40, 35, 30, 40, 30, 30, 40, 35, 35, 50, 30, 30, 30, 30, 35, 35][i], mix=[28, 25, 24, 30, 30, 26, 26, 30, 26, 28, 30, 26, 28, 30, 24, 34, 22, 30, 28, 28][i],
      rev=[100, 0, 100, 100, 50, 100, 0, 50, 100, 60, 100, 100, 100, 0, 0, 100, 100, 100, 70, 70][i], spray=[0, 0, 30, 0, 40, 0, 0, 50, 0, 70, 0, 20, 0, 0, 0, 0, 0, 0, 20, 20][i],
      fb=PING if 'Ping' in nm else MIXED, dyn=2 if 'Ducked' in nm else 0, dthr=-24, ssync=(D8 + 1) if 'Eighth' in nm else (D16 + 1) if 'Sixteenth' in nm or 'Groove' in nm else 0, lc=80)
# Ambient (20)
for i, nm in enumerate(["Distant Bells", "Underwater Glass", "Night Sky", "Cave Drips", "Wind Chimes", "Aurora", "Deep Space", "Morning Mist", "Rain On Glass",
                        "Floating", "Slow Motion", "Echo Valley", "Glass Forest", "Hollow", "Tidal", "Lantern", "Satellite", "Dusk", "Fog Horn", "Weightless"]):
    S("Ambient | " + nm, [24, -12, 12, 19, 24, 12, 12, 7, 24, 12, 0, 0, 12, -12, 5, 12, 24, 7, -24, 12][i], [500, 900, 1200, 300, 250, 1500, 2500, 1000, 200, 1800, 3000, 600, 800, 1400, 2000, 700, 400, 1600, 1200, 2200][i],
      delay=[200, 300, 500, 100, 150, 400, 800, 300, 100, 600, 1000, 900, 250, 500, 700, 300, 400, 600, 300, 800][i], rec=[60, 70, 75, 55, 55, 75, 85, 65, 50, 80, 80, 70, 70, 75, 80, 65, 60, 75, 70, 85][i],
      mix=28, rev=50, eng=CLN if i % 2 else VIN, spray=[40, 60, 50, 30, 50, 60, 70, 50, 70, 60, 40, 30, 60, 50, 60, 40, 50, 50, 30, 70][i], dens=2, smooth=45, hc=10000, fb=DUAL if i % 3 == 0 else MIXED,
      sor=15 if i % 3 == 0 else 0, dor=90 if i % 3 == 0 else 0, l1=(T_SP, SINE, 0, 30, 0.1))
# Sound Design (12)
for i, nm in enumerate(["Alien Transmission", "Monster Rise", "Black Hole", "Robot Choir", "Infinite Climb", "Collapse", "Sci-Fi Scanner", "Haunted Music Box",
                        "Time Slip", "Gravity Well", "Mutant Echo", "Siren Shards"]):
    S("Sound Design | " + nm, [36, -24, -48, 12, 12, -36, 24, 24, 0, -24, 7, 12][i], [200, 600, 1500, 150, 400, 800, 80, 300, 1000, 1200, 250, 300][i],
      rec=[85, 70, 90, 70, 92, 80, 60, 70, 75, 85, 80, 70][i], mix=45, rev=[50, 0, 100, 0, 0, 100, 0, 100, 100, 100, 50, 0][i], spray=[60, 0, 50, 20, 0, 40, 30, 20, 70, 40, 60, 0][i], dens=2,
      l1=(T_P, SQR, 0, 25, 4.0) if 'Siren' in nm or 'Scanner' in nm else (T_P, RUP, 0, 20, 0.1) if 'Climb' in nm else (T_P, SH, 0, 15, 3.0) if 'Mutant' in nm else (0, SINE, 0, 0, 0.5),
      l2=(T_SP, SINE, 0, 50, 0.2) if i % 2 else (0, TRI, 0, 0, 2.0))

#=====================================================================================================================
# Murmuration. sync: 0 free / division + 1; shimmerSt 0 -24, 1 -12, 2 +12, 3 +24
plug('Murmuration', 'SqMurmurationPlugin', ['sync', 'modShape', 'shimmerSt', 'freezeOn', 'freezeMode', 'infiniteOn', 'customEnvOn'])
def M(name, time=2000, x=25, y=50, tex=60, warp=30, color=0, blur=40, grain=0, mrate=0.3, mdepth=20, shim=0, sst=2, sfade=50, tilt=0, lc=20, hc=20000,
      width=100, duck=0, pre=0, mix=40, beats=0, sync=0, mshape=SINE, inf=0):
    vals = dict(time=time, beats=beats, x=x, y=y, texture=tex, warp=warp, color=color, blur=blur, grain=grain, modRate=mrate, modDepth=mdepth, shimmer=shim,
                shimmerFade=sfade, tilt=tilt, lowCut=lc, highCut=hc, width=width, duck=duck, preDelay=pre, mix=mix)
    add('Murmuration', name, vals, dict(sync=sync, modShape=mshape, shimmerSt=sst, freezeOn=0, freezeMode=0, infiniteOn=inf, customEnvOn=0))
for i, nm in enumerate(["Gentle Swell", "Slow Rise", "Bowed Swell", "Breath In", "Rising Tide", "Soft Crescendo", "Wave Builder", "Velvet Swell", "Glass Swell", "Dark Swell", "Choir Swell", "Pad Swell"]):
    M("Swells | " + nm, time=[1500, 2500, 1800, 1200, 3000, 2000, 2200, 1600, 1400, 2600, 2400, 2000][i], x=[60, 75, 65, 55, 80, 70, 72, 62, 58, 78, 70, 66][i], y=[60, 70, 55, 50, 75, 65, 68, 55, 60, 70, 62, 58][i],
      tex=70, color=[0, 0, 10, 0, -10, 0, 0, -20, 30, -40, 10, 0][i], blur=50, mix=38, hc=[20000, 16000, 14000, 18000, 12000, 16000, 15000, 10000, 20000, 6000, 14000, 12000][i])
for i, nm in enumerate(["Reverse Build", "Backwards Bloom", "Suck In", "Reverse Wash", "Inhale", "Pre-Echo", "Reverse Cloud", "Tension Riser", "Reverse Halo", "Pull Back", "Tape Suck", "Reverse Fog"]):
    M("Reverse Builds | " + nm, time=[1200, 2000, 800, 2500, 1000, 600, 3000, 4000, 1800, 1400, 900, 2800][i], x=[95, 92, 98, 90, 96, 99, 88, 97, 93, 94, 98, 90][i], y=[85, 80, 90, 70, 85, 95, 75, 90, 80, 85, 92, 70][i],
      tex=[80, 85, 70, 90, 75, 60, 90, 85, 85, 75, 65, 90][i], blur=40, mix=40, shim=[0, 0, 0, 0, 0, 0, 0, 30, 40, 0, 0, 0][i])
for i, nm in enumerate(["Wide Wash", "Warm Wash", "Bright Wash", "Dark Wash", "Ocean Wash", "Velvet Wash", "Endless Wash", "Silk Wash", "Fog Wash", "Haze Wash", "Rain Wash", "Ambient Wash"]):
    M("Washes | " + nm, time=[3000, 2500, 2000, 4000, 5000, 3500, 6000, 2800, 4500, 3200, 2200, 5500][i], x=[20, 25, 15, 30, 35, 25, 30, 20, 40, 30, 15, 35][i], y=[25, 30, 20, 35, 30, 25, 20, 30, 25, 20, 30, 25][i],
      tex=95, blur=70, color=[0, -20, 30, -50, -10, -30, -20, 0, -40, -10, 20, -15][i], mix=[35, 35, 30, 35, 32, 35, 30, 34, 32, 34, 32, 30][i], width=[150, 120, 140, 110, 160, 120, 150, 130, 120, 140, 130, 150][i],
      mdepth=30, hc=[18000, 12000, 20000, 6000, 10000, 9000, 12000, 16000, 7000, 11000, 15000, 10000][i])
for i, nm in enumerate(["Cumulus", "Cirrus", "Thunderhead", "Nimbus", "Stratus", "Mammatus", "Silver Lining", "Storm Front", "Cotton", "Vapour", "Plume", "Billow"]):
    M("Clouds | " + nm, time=[1500, 900, 3000, 2200, 2600, 1800, 1200, 3500, 1000, 700, 2000, 2400][i], x=[30, 10, 45, 35, 40, 50, 20, 55, 25, 15, 30, 40][i], y=[50, 60, 40, 50, 30, 60, 55, 45, 50, 70, 45, 40][i],
      tex=[60, 50, 75, 65, 70, 55, 50, 80, 55, 45, 65, 70][i], blur=[40, 30, 60, 50, 50, 40, 30, 70, 40, 30, 50, 60][i], grain=[0, 20, 0, 0, 0, 30, 0, 10, 0, 40, 0, 0][i], mix=40, mdepth=25)
for i, nm in enumerate(["Shimmer Octave", "Shimmer Heaven", "Two Octaves Up", "Sub Shimmer", "Glass Shimmer", "Late Shimmer", "Shimmer Swell", "Cathedral Shimmer", "Shimmer Mist", "Crystal Shimmer", "Down Shimmer", "Halo Shimmer"]):
    M("Shimmer | " + nm, time=[2500, 4000, 2000, 3000, 1500, 3500, 2500, 5000, 3000, 1800, 2800, 3200][i], x=[25, 30, 20, 30, 15, 40, 70, 35, 25, 20, 30, 30][i], y=45, tex=80, blur=60,
      shim=[60, 80, 50, 50, 70, 90, 60, 75, 50, 65, 55, 70][i], sst=[2, 2, 3, 1, 3, 2, 2, 2, 2, 3, 1, 2][i], sfade=[50, 70, 40, 60, 30, 90, 60, 70, 50, 40, 60, 60][i], mix=34, hc=16000)
for i, nm in enumerate(["Infinite Pad", "Endless Drone", "Held Chord", "Eternal Glass", "Infinite Shimmer", "Never Fade", "Drone Bed", "Hold Forever", "Infinite Dark", "Infinite Air", "Sustain Cloud", "Infinite Haze"]):
    M("Freeze Pads | " + nm, time=[1500, 2000, 1200, 1000, 2500, 1800, 3000, 1400, 2200, 1600, 2600, 2000][i], x=30, y=30, tex=90, blur=70, mix=[30, 28, 30, 30, 26, 30, 28, 30, 28, 30, 28, 28][i],
      shim=[0, 0, 0, 40, 60, 0, 0, 0, 0, 30, 0, 0][i], color=[0, -20, 0, 20, 0, 0, -30, 0, -60, 30, 0, -10][i], hc=[14000, 9000, 12000, 18000, 14000, 12000, 8000, 12000, 5000, 18000, 12000, 10000][i], inf=1)
for i, nm in enumerate(["Quarter Swarm", "Eighth Swarm", "Bar Cloud", "Half Bar Bloom", "Dotted Swarm", "Triplet Swarm", "Two Bar Swell", "Rhythmic Taps", "Sixteenth Spray", "Beat Smear", "Synced Reverse", "Four Bar Wash"]):
    M("Rhythmic | " + nm, beats=[1, 0.5, 4, 2, 0.75, 0, 8, 2, 0, 1, 2, 16][i], sync=[0, 0, 0, 0, 0, D8T + 1, 0, 0, D16 + 1, 0, 0, 0][i], x=[20, 15, 30, 40, 20, 20, 70, 10, 10, 30, 95, 35][i], y=[60, 60, 40, 50, 60, 60, 60, 80, 70, 40, 85, 30][i],
      tex=[10, 15, 35, 30, 15, 20, 60, 0, 25, 50, 70, 85][i], blur=[10, 10, 30, 30, 15, 10, 50, 0, 10, 50, 40, 70][i], mix=36, mdepth=10)
for i, nm in enumerate(["Vocal Halo", "Lead Glow", "Ad-Lib Cloud", "Ballad Space", "Rap Smear", "Choir Halo", "Breath Cloud", "Hook Shimmer", "Vocal Reverse In", "Vocal Wash", "Whisper Halo", "Gospel Air"]):
    M("Vocal Halos | " + nm, time=[1800, 1500, 1200, 2500, 800, 2200, 1400, 2000, 1500, 3000, 1200, 2400][i], x=[20, 25, 15, 30, 10, 30, 20, 25, 95, 30, 15, 30][i], y=[55, 50, 60, 45, 70, 40, 55, 50, 85, 30, 60, 40][i],
      tex=75, blur=55, shim=[0, 0, 0, 0, 0, 30, 0, 50, 0, 0, 0, 25][i], lc=180, hc=12000, duck=[6, 6, 4, 4, 9, 4, 4, 6, 3, 4, 6, 4][i], mix=[30, 28, 32, 30, 26, 32, 30, 28, 34, 30, 30, 30][i])
for i, nm in enumerate(["Snare Smear", "Kick Cloud", "Room Smear", "Overhead Haze", "Break Smear", "Hat Spray", "Clap Cloud", "Tom Bloom", "Drum Bus Air", "Loop Haze", "Reverse Snare", "Perc Cloud"]):
    M("Drum Smears | " + nm, time=[600, 400, 800, 1200, 500, 300, 700, 900, 1000, 1100, 500, 600][i], x=[10, 5, 15, 20, 10, 5, 10, 15, 20, 25, 95, 10][i], y=[70, 80, 60, 50, 70, 80, 70, 60, 50, 50, 85, 70][i],
      tex=[70, 60, 80, 85, 75, 50, 70, 75, 80, 85, 70, 65][i], blur=50, lc=[150, 40, 100, 300, 100, 600, 150, 80, 100, 100, 150, 200][i], mix=[30, 25, 30, 26, 30, 26, 30, 28, 24, 28, 32, 28][i], duck=[4, 6, 3, 0, 4, 0, 4, 3, 0, 0, 0, 3][i])
for i, nm in enumerate(["Granular Storm", "Ninety Second Drift", "Alien Swarm", "Time Smear", "Warp Field", "Broken Cloud", "Particle Rain", "Sub Swarm", "Ghost Cloud", "Glitch Murmur", "Spectral Ghost", "Collapse"]):
    M("Sound Design | " + nm, time=[1500, 90000, 2000, 5000, 3000, 1200, 800, 2500, 4000, 600, 3000, 2000][i], x=[30, 10, 50, 40, 30, 50, 20, 30, 60, 20, 40, 95][i], y=[50, 30, 70, 30, 50, 60, 60, 40, 50, 70, 40, 90][i],
      tex=[30, 100, 50, 90, 60, 40, 20, 80, 70, 10, 85, 80][i], grain=[90, 0, 60, 30, 0, 80, 70, 0, 40, 90, 0, 50][i], warp=[30, 80, 60, 100, 100, 30, 30, 30, 60, 20, 30, 80][i],
      mrate=[3, 0.1, 8, 0.2, 2, 5, 6, 0.3, 0.5, 12, 0.2, 1][i], mdepth=[60, 30, 90, 50, 100, 70, 50, 30, 60, 80, 40, 60][i], mshape=[SH, SINE, SRND, SINE, TRI, SH, SH, SINE, SRND, SQR, SINE, RDN][i],
      shim=[0, 30, 40, 0, 0, 0, 30, 0, 50, 0, 0, 0][i], sst=[2, 2, 3, 2, 2, 2, 3, 0, 1, 2, 2, 2][i], color=[0, -30, 40, -60, 0, 0, 20, -70, -20, 0, 0, -40][i], mix=40)

#=====================================================================================================================
# Masquerade. mode 0 Transpose 1 Quantize 2 Robot 3 MIDI; engine 0 Live 1 Studio; driveVoicing 0 off, 1 Clean 2 Fat 3 Squash 4 Dirt 5 Crunch 6 Shred 7 Pump
plug('Masquerade', 'SqMasqueradePlugin', ['mode', 'linkOn', 'engine', 'voiceType', 'autoKeyOn', 'keyRoot', 'scale', 'robotNote', 'driveVoicing'])
TR, QU, RO, MI = 0, 1, 2, 3
def Q(name, mode=TR, pitch=0, formant=0, link=0, drive=40, dv=0, retune=0, cl=70, cs=60, mix=100, eng=0, vt=1, robot=48):
    add('Masquerade', name, dict(pitch=pitch, formant=formant, drive=drive, retune=retune, chordLevel=cl, chordSpread=cs, mix=mix),
        dict(mode=mode, linkOn=link, engine=eng, voiceType=vt, autoKeyOn=1, keyRoot=0, scale=1, robotNote=robot, driveVoicing=dv))
for i, (nm, p, f, dv, d) in enumerate([("Demon", -12, -6, 5, 50), ("Ogre", -9, -9, 4, 40), ("Beast", -7, -5, 6, 35), ("Giant", -12, -12, 0, 0), ("Troll", -5, -8, 4, 45),
                                       ("Hell Voice", -14, -4, 6, 60), ("Golem", -10, -12, 3, 40), ("Dragon", -12, -3, 6, 55), ("Undead", -8, -2, 5, 40), ("Brute", -6, -7, 4, 50),
                                       ("Cave Thing", -16, -10, 2, 30), ("Possessed", -12, 2, 5, 45), ("Swamp Monster", -11, -7, 4, 35), ("Abyss", -20, -14, 6, 40)]):
    Q("Monster | " + nm, pitch=p, formant=f, dv=dv, drive=d, eng=1 if i % 3 == 0 else 0)
for i, (nm, p, f) in enumerate([("Chipmunk", 12, 12), ("Squeaky", 9, 10), ("Cartoon Mouse", 14, 14), ("Helium", 7, 12), ("Tiny Friend", 10, 6), ("Gremlin", 8, 4),
                                ("Sped Up Hook", 5, 5), ("Pixie", 16, 14), ("Squeak Lite", 12, 8), ("Baby Voice", 10, 9), ("Fairy", 19, 12), ("Elf", 6, 8)]):
    Q("Chipmunk | " + nm, pitch=p, formant=f, link=1 if 'Sped Up' in nm else 0)
for i, (nm, p, f) in enumerate([("Deeper Male", -3, -3), ("Lighter Female", 3, 3), ("Male To Female", 5, 4), ("Female To Male", -5, -4), ("Baritone", -5, -2), ("Tenor Lift", 3, 1),
                                ("Contralto", -2, -3), ("Soprano Lift", 4, 3), ("Androgynous", 0, 2), ("Young Voice", 2, 4), ("Older Voice", -2, -2), ("Narrator Depth", -4, -3),
                                ("Formant Only Up", 0, 4), ("Formant Only Down", 0, -4)]):
    Q("Gender | " + nm, pitch=p, formant=f, eng=1)
for i, (nm, note, dv, d) in enumerate([("Classic Robot", 48, 0, 0), ("Low Robot", 36, 0, 0), ("High Robot", 60, 0, 0), ("Tin Overlord", 45, 5, 40), ("Android", 52, 1, 30),
                                       ("Cyborg", 43, 4, 35), ("Synth Talk", 57, 2, 30), ("Machine Monk", 38, 3, 30), ("Computer", 55, 0, 0), ("Broken Bot", 47, 6, 45),
                                       ("Robot Bass", 31, 4, 40), ("Robot Choir", 50, 2, 25)]):
    Q("Robot | " + nm, mode=RO, robot=note, dv=dv, drive=d, formant=[0, -2, 2, -3, 0, -4, 1, -5, 0, -2, -6, 1][i])
for i, (nm, p, f) in enumerate([("Throat Swap", 0, -7), ("Nasal", 0, 6), ("Hollow", 0, -10), ("Tiny Head", 0, 12), ("Huge Chest", 0, -12), ("Formant Wobble", 0, 5),
                                ("Duck Talk", 4, 14), ("Alien", 7, -9), ("Insect", 10, 14), ("Underworld", -3, -16), ("Twisted", 5, -6), ("Shrunken", -2, 9)]):
    Q("Formant FX | " + nm, pitch=p, formant=f, eng=1 if i % 2 else 0, dv=4 if 'Twisted' in nm else 0, drive=30)
for i, (nm, p, mode) in enumerate([("Octave Up", 12, TR), ("Octave Down", -12, TR), ("Two Octaves Down", -24, TR), ("Two Octaves Up", 24, TR), ("Octave Up In Key", 12, QU),
                                   ("Octave Down In Key", -12, QU), ("Octave Up Linked", 12, TR), ("Octave Down Linked", -12, TR), ("Fifth Up In Key", 7, QU), ("Fourth Down In Key", -5, QU)]):
    Q("Octave | " + nm, mode=mode, pitch=p, link=1 if 'Linked' in nm else 0, eng=1)
for i, (nm, p, mode, rt) in enumerate([("Hard Tune Chop", 0, QU, 0), ("Pitched Chop Up", 5, QU, 0), ("Pitched Chop Down", -7, QU, 0), ("Hook Lift", 7, QU, 5), ("Ad-Lib Fifth", 7, TR, 0),
                                       ("Vocal Chop Octave", 12, QU, 0), ("Soft Snap", 0, QU, 30), ("Chord Stab", 0, MI, 0), ("MIDI Melody", 0, MI, 0), ("MIDI Chords Wide", 0, MI, 0),
                                       ("Low Chop", -12, QU, 0), ("Glide Chop", 0, QU, 80)]):
    Q("Vocal Chop | " + nm, mode=mode, pitch=p, retune=rt, cl=80 if 'Chord' in nm else 0, cs=90 if 'Wide' in nm else 50)
for i, (nm, p, f, dv, d, mode) in enumerate([("Radio Ghost", -3, 6, 4, 50, TR), ("Possessed Choir", -12, 0, 5, 40, MI), ("Glitch Spirit", 12, -12, 6, 50, TR), ("Megaphone Robot", 0, 4, 5, 70, RO),
                                             ("Subterranean", -24, -18, 3, 40, TR), ("Squeaky Demon", 12, -12, 5, 50, TR), ("Crushed Mutant", -7, 7, 6, 80, TR), ("Wobble Creature", 3, -3, 4, 50, TR),
                                             ("Pump Monster", -12, -6, 7, 60, TR), ("Fat Robot", 0, -3, 2, 60, RO), ("Crunch Choir", 0, 0, 5, 50, MI), ("Alien Pilot", 9, -11, 4, 45, TR),
                                             ("Space Station", -5, 9, 1, 30, RO), ("Nightmare", -18, 8, 6, 60, TR)]):
    Q("Sound Design | " + nm, mode=mode, pitch=p, formant=f, dv=dv, drive=d, robot=45)

#=====================================================================================================================
WANT = {'Shard': 220, 'Murmuration': 120, 'Masquerade': 100}

def specs_defaults():
    src = open(os.path.join(ROOT, 'Source', 'SqColor111c.h')).read()
    d = {}
    for nm, (cls, _, _) in PLUGS.items():
        body = src[src.index('class ' + cls):]; body = body[:body.index('return s;')]
        d[nm] = {m.group(1): float(m.group(2).rstrip('f')) for m in re.finditer(r'\{ "(\w+)",\s*"[^"]+",\s*[-\d.f]+,\s*[-\d.f]+,\s*([-\d.f]+),', body)}
    return d

def write_table(path, defs):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111e.py - do not edit.\n#pragma once\n')
        f.write('struct D2Row { const char* name; float v[32]; int pr[20]; };\n')
        for nm, (cls, props, lst) in PLUGS.items():
            allp = list(defs[nm].keys())
            f.write('// %s values: %s | props: %s\n' % (nm, ' '.join(allp), ' '.join(props)))
            f.write('static const D2Row %sRows[] = {\n' % nm.lower())
            for p in lst:
                for k in p['vals']: assert k in allp, (nm, k)
                v = [p['vals'].get(k, defs[nm][k]) for k in allp]
                pr = [int(p['props'].get(k, 0)) for k in props]
                f.write('    { %s, { %s }, { %s } },\n' % (json.dumps(p['name']), ', '.join('%g' % x for x in v), ', '.join(str(x) for x in pr)))
            f.write('};\n')

def calibrate():
    out = subprocess.check_output(['bash', os.path.join(ROOT, 'scripts', 'build_calib_d2.sh')], text=True).strip().splitlines()[-1]
    res = subprocess.check_output([out], text=True)
    trims = {}
    for line in res.splitlines():
        if line.startswith('TRIM\t'):
            _, plug_, name, trim, peak = line.split('\t')
            trims[(plug_, name)] = (float(trim), float(peak))
    return trims

def num(v):
    s = ('%.2f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def main():
    for nm, (cls, props, lst) in PLUGS.items():
        names = [p['name'] for p in lst]
        dup = [n for n in names if names.count(n) > 1]
        assert not dup, 'duplicate preset names in %s: %s' % (nm, dup[:3])
        cats = sorted(set(n.split(' | ')[0] for n in names))
        print('%s: %d presets (want %d) in %d categories: %s' % (nm, len(lst), WANT[nm], len(cats), ', '.join(cats)))
        assert len(lst) == WANT[nm], nm
    defs = specs_defaults()
    write_table(os.path.join(ROOT, 'tests', 'presets111e_table.h'), defs)
    trims = calibrate()
    fns, adds, worst = [], [], 0
    for k, (nm, (cls, props, lst)) in enumerate(PLUGS.items()):
        lines = []
        for p in lst:
            trim, peak = trims[(nm, p['name'])]
            trim = max(-24.0, min(12.0, min(trim, 3.0 - peak))); worst = max(worst, abs(trim))
            vals = ', '.join('N (%s, %s)' % (json.dumps(kk), num(vv)) for kk, vv in p['vals'].items()) + ', N ("output", %s)' % num(trim)
            prs = ', '.join('{ "%s", %d }' % (kk, int(vv)) for kk, vv in p['props'].items())
            lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111e_%d()   // %s\n{\n    return {\n%s\n    };\n}\n' % (k + 1, nm, '\n'.join(lines)))
        adds.append('    m[%s] = presets111e_%d();\n' % (json.dumps(nm), k + 1))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111e.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111e.py - factory presets for the 1.11 Color & Space second wave (part 2). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111d.inl; bank() calls add111e().\n')
        f.write('// Output = the measured trim that holds the level on the preset\'s material (vocal / drums / pink noise).\n\n')
        f.write('\n'.join(fns))
        f.write('inline void add111e (std::map<juce::String, std::vector<Preset>>& m)\n{\n' + ''.join(adds) + '}\n')
    print('wrote', out, '- largest trim %.2f dB' % worst)

if __name__ == '__main__':
    main()
