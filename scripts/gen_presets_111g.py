#!/usr/bin/env python3
"""Factory presets for the 1.11 Voice second wave, part 1 (super prompt V8, V11, V12, V13):
   Siren Verb (140), Mic Closet (80), Larynx (60), Exhale (30).

Every preset is rendered through the real DSP (tests/preset_calib_e1.cpp, built against JUCE core by
scripts/build_voice111e_test.sh because the voice core is Moth Tune's tracker) and its Output trim is written from the
measurement so it holds its level on the sung vocal (all four are vocal plug-ins), with the peak kept under +3 dBFS.

Writes tests/presets111g_table.h and Source/SignaturePresets111g.inl (add111g(), called from SignaturePresets::bank()).
"""
import json, os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGS = {}
def plug(name, values, props): PLUGS[name] = (values, props, [])
def add(name, preset, vals, props):
    values, propids, lst = PLUGS[name]
    for k in vals: assert k in values, (name, k)
    for k in props: assert k in propids, (name, k)
    assert ' | ' in preset, preset
    lst.append(dict(name=preset, vals=vals, props=props))

#=====================================================================================================================
# Siren Verb
HALL, PLATE, ROOM, CHAMBER, SPRING, AMB, SHIM = range(7)
SV = ['size', 'decay', 'preDelay', 'damping', 'early', 'modRate', 'modDepth', 'width', 'lowCut', 'highCut', 'mix', 'output', 'prePitch', 'throat', 'tube',
      'deEss', 'reverseTime', 'autoEqDepth', 'tone', 'comp', 'gate', 'duck', 'shimmer', 'delayTime', 'delayFeedback', 'delayLevel']
SVP = ['algo', 'reverseOn', 'autoEqOn', 'delayMode', 'delaySync', 'shimmerPitch']
plug('Siren Verb', SV, SVP)
def S(name, algo, decay, mix, size=50, pre=20, damp=50, early=50, lc=150, hc=11000, eq=1, eqd=9, tone=0, deess=30, duck=0, comp=0, gate=0, width=100,
      mod=30, rate=0.6, pitch=0, throat=0, tube=0, rev=0, revt=400, shim=35, shp=1, dmode=0, dsync=0, dtime=375, dfb=30, dlev=30):
    add('Siren Verb', name, dict(size=size, decay=decay, preDelay=pre, damping=damp, early=early, modRate=rate, modDepth=mod, width=width, lowCut=lc, highCut=hc,
                                 mix=mix, prePitch=pitch, throat=throat, tube=tube, deEss=deess, reverseTime=revt, autoEqDepth=eqd, tone=tone, comp=comp, gate=gate,
                                 duck=duck, shimmer=shim, delayTime=dtime, delayFeedback=dfb, delayLevel=dlev),
        dict(algo=algo, reverseOn=rev, autoEqOn=eq, delayMode=dmode, delaySync=dsync, shimmerPitch=shp))
# sync prop = division + 1: 1/16 7, 1/8 10, 1/8. 11, 1/4 13, 1/4. 14, 1/2 16
# Vocal Plates (18)
for nm, dec, mx, damp, pre, tone in [("Classic Lead Plate", 1.8, 22, 45, 25, 10), ("Bright Pop Plate", 1.5, 20, 30, 20, 30), ("Dark Velvet Plate", 2.2, 22, 70, 30, -30),
                                     ("Short Tuck Plate", 0.9, 18, 50, 10, 0), ("Long Ballad Plate", 3.2, 24, 50, 40, 0), ("Airy Female Plate", 1.6, 20, 35, 25, 35),
                                     ("Thick Male Plate", 2.0, 22, 60, 20, -15), ("R&B Silk Plate", 2.4, 24, 45, 35, 15), ("Rap Tight Plate", 0.8, 14, 55, 8, 0),
                                     ("Backing Stack Plate", 2.0, 30, 50, 15, 0), ("Ad-Lib Splash Plate", 1.2, 26, 40, 5, 20), ("Vintage Steel Plate", 2.6, 22, 55, 15, -10),
                                     ("Gospel Lift Plate", 2.8, 24, 45, 30, 10), ("Whisper Plate", 1.4, 18, 40, 15, 25), ("Wide Hook Plate", 2.0, 24, 45, 25, 10),
                                     ("Gentle Duck Plate", 2.2, 26, 50, 20, 0), ("Pitch Lock Plate", 2.0, 24, 45, 25, 0), ("Crisp Narrator Plate", 0.7, 12, 50, 5, 0)]:
    S("Vocal Plates | " + nm, PLATE, dec, mx, damp=damp, pre=pre, tone=tone, duck=40 if 'Duck' in nm else 0, width=150 if 'Wide' in nm else 100, eqd=14 if 'Pitch Lock' in nm else 9)
# Vocal Halls (18)
for nm, dec, mx, size, pre, damp in [("Concert Hall Lead", 2.8, 22, 70, 35, 50), ("Big Pop Hall", 3.2, 24, 80, 40, 45), ("Dark Cinema Hall", 4.0, 22, 90, 50, 70),
                                     ("Bright Arena", 2.6, 22, 85, 30, 30), ("Intimate Small Hall", 1.6, 18, 40, 20, 50), ("Ballad Hall", 3.6, 24, 75, 50, 50),
                                     ("Choir Hall", 3.0, 30, 80, 25, 45), ("Velvet Hall", 2.4, 22, 60, 30, 65), ("Huge Epic Hall", 6.0, 22, 100, 60, 50),
                                     ("Pre-Delay Throw Hall", 2.5, 22, 70, 120, 50), ("Stadium Echo Hall", 3.4, 22, 95, 80, 45), ("Soft Pad Hall", 4.5, 26, 85, 30, 60),
                                     ("Warm Wood Hall", 2.2, 20, 55, 25, 60), ("Glass Hall", 2.8, 20, 70, 30, 25), ("Ducked Hook Hall", 3.0, 28, 75, 30, 50),
                                     ("Gated Tail Hall", 2.5, 24, 70, 20, 50), ("Long Fade Hall", 7.5, 20, 100, 40, 55), ("Modulated Hall", 3.0, 22, 75, 30, 45)]:
    S("Vocal Halls | " + nm, HALL, dec, mx, size=size, pre=pre, damp=damp, duck=45 if 'Duck' in nm else 0, gate=55 if 'Gated' in nm else 0, mod=70 if 'Modulated' in nm else 30, rate=1.1 if 'Modulated' in nm else 0.6)
# Vocal Rooms & Chambers (16)
for nm, algo, dec, mx, size, pre in [("Studio Booth Room", ROOM, 0.5, 14, 20, 5), ("Live Room", ROOM, 0.9, 18, 45, 10), ("Bright Drum Room Voice", ROOM, 0.7, 16, 35, 5),
                                     ("Bedroom Room", ROOM, 0.4, 14, 15, 0), ("Tile Room", ROOM, 1.0, 16, 30, 5), ("Big Wooden Room", ROOM, 1.3, 18, 70, 15),
                                     ("Club Room", ROOM, 1.1, 18, 60, 10), ("Garage Room", ROOM, 1.4, 18, 55, 5),
                                     ("Echo Chamber", CHAMBER, 1.6, 20, 55, 15), ("Dark Stone Chamber", CHAMBER, 2.2, 20, 70, 20), ("Bright Chamber", CHAMBER, 1.4, 18, 45, 10),
                                     ("Soul Chamber", CHAMBER, 1.9, 22, 60, 25), ("Small Chamber", CHAMBER, 1.0, 18, 30, 10), ("Basement Chamber", CHAMBER, 2.4, 20, 80, 15),
                                     ("Velvet Chamber", CHAMBER, 1.7, 20, 55, 20), ("Chamber Doubler", CHAMBER, 0.8, 22, 25, 30)]:
    S("Vocal Rooms | " + nm, algo, dec, mx, size=size, pre=pre, early=70, damp=55)
# Vocal Ambience (10)
for nm, dec, mx, size, early in [("Barely There", 0.3, 12, 20, 70), ("Natural Air", 0.5, 14, 35, 80), ("Close Ambience", 0.35, 14, 15, 90), ("Spoken Word Air", 0.4, 10, 25, 70),
                                 ("Podcast Space", 0.3, 10, 20, 60), ("Wide Ambience", 0.6, 16, 45, 80), ("Warm Ambience", 0.5, 14, 35, 75), ("Bright Ambience", 0.45, 14, 30, 85),
                                 ("Thick Ambience", 0.8, 18, 55, 90), ("Tight Ad-Lib Space", 0.3, 16, 15, 70)]:
    S("Vocal Ambience | " + nm, AMB, dec, mx, size=size, early=early, pre=0, width=160 if 'Wide' in nm else 100)
# Vocal Springs (10)
for nm, dec, mx, tube, damp in [("Surf Spring", 2.2, 24, 20, 50), ("Soul Spring", 1.6, 20, 30, 55), ("Dub Spring", 2.8, 26, 25, 60), ("Bright Twang Spring", 1.8, 22, 10, 35),
                                ("Short Spring", 1.0, 18, 15, 55), ("Rockabilly Spring", 1.4, 22, 40, 45), ("Dark Tank Spring", 2.4, 22, 20, 75), ("Garage Spring", 1.8, 24, 50, 50),
                                ("Lo-Fi Spring", 1.6, 24, 60, 70), ("Psych Spring", 3.0, 26, 30, 45)]:
    S("Vocal Springs | " + nm, SPRING, dec, mx, tube=tube, damp=damp, early=10)
# Vocal Shimmer & Pads (14)
for nm, dec, mx, shim, shp, size in [("Octave Shimmer", 4.5, 24, 35, 1, 85), ("Angel Shimmer", 6.0, 26, 45, 1, 95), ("Fifth Shimmer", 4.0, 22, 30, 0, 80), ("Two Octave Halo", 5.5, 22, 30, 3, 90),
                                     ("Subtle Shimmer", 3.0, 18, 20, 1, 70), ("Dark Below Shimmer", 4.5, 22, 30, 4, 85), ("Twelfth Shimmer", 5.0, 22, 30, 2, 90),
                                     ("Ambient Bloom", 7.0, 24, 40, 1, 100), ("Choir Shimmer", 5.0, 30, 35, 1, 90), ("Hook Shimmer", 3.5, 22, 30, 1, 75),
                                     ("Infinite Pad", 12.0, 26, 50, 1, 100), ("Cloud Pad", 9.0, 26, 40, 0, 100), ("Short Sparkle", 2.0, 18, 25, 1, 55), ("Ducked Shimmer", 5.0, 28, 35, 1, 90)]:
    S("Vocal Shimmer | " + nm, SHIM, dec, mx, shim=shim, shp=shp, size=size, pre=40, duck=45 if 'Ducked' in nm else 0, damp=45)
# Vocal Echo + Verb (16)
for nm, algo, dec, mx, dmode, dsync, dfb, dlev in [("Quarter Echo Plate", PLATE, 1.6, 22, 1, 13, 30, 30), ("Eighth Echo Hall", HALL, 2.4, 22, 1, 10, 30, 28),
                                                   ("Dotted Echo Plate", PLATE, 1.8, 22, 1, 11, 35, 30), ("Ping-Pong Hall", HALL, 2.6, 22, 2, 10, 40, 30),
                                                   ("Ping-Pong Plate", PLATE, 1.8, 22, 2, 13, 35, 30), ("Slap Room", ROOM, 0.8, 20, 1, 0, 5, 35),
                                                   ("Long Throw Hall", HALL, 3.0, 22, 2, 16, 50, 28), ("Dub Echo Spring", SPRING, 2.2, 24, 1, 14, 55, 32),
                                                   ("Half Note Wash", HALL, 3.5, 22, 1, 16, 45, 28), ("Triplet Echo Plate", PLATE, 1.6, 22, 1, 12, 35, 30),
                                                   ("Wide Echo Chamber", CHAMBER, 1.8, 22, 2, 7, 30, 26), ("Sixteenth Shimmer Echo", SHIM, 4.0, 22, 2, 7, 35, 26),
                                                   ("Hip-Hop Echo Room", ROOM, 0.7, 18, 1, 13, 25, 30), ("R&B Echo Hall", HALL, 2.6, 22, 2, 11, 35, 28),
                                                   ("Pop Hook Echo", PLATE, 1.6, 22, 2, 10, 30, 28), ("Ballad Echo Hall", HALL, 3.2, 22, 1, 13, 40, 26)]:
    S("Vocal Echo + Verb | " + nm, algo, dec, mx, dmode=dmode, dsync=dsync, dfb=dfb, dlev=dlev, dtime=120 if dsync == 0 else 375, duck=30)
# Rap Vocal (12)
for nm, algo, dec, mx, pre, duck in [("Dry Booth Air", AMB, 0.3, 10, 0, 20), ("Trap Lead Room", ROOM, 0.6, 14, 5, 30), ("Boom Bap Plate", PLATE, 0.9, 14, 10, 30),
                                     ("Drill Dark Room", ROOM, 0.8, 14, 5, 35), ("Ad-Lib Hall Throw", HALL, 2.0, 26, 20, 50), ("Melodic Rap Plate", PLATE, 1.4, 18, 15, 35),
                                     ("Hook Double Chamber", CHAMBER, 1.0, 18, 10, 30), ("Club Rap Room", ROOM, 1.0, 16, 5, 40), ("Grime Tile Room", ROOM, 0.7, 14, 0, 30),
                                     ("Lo-Fi Rap Spring", SPRING, 1.2, 16, 5, 30), ("Wide Ad-Lib Ambience", AMB, 0.5, 20, 0, 30), ("Emo Rap Hall", HALL, 2.8, 22, 30, 45)]:
    S("Rap Vocal | " + nm, algo, dec, mx, pre=pre, duck=duck, comp=25, deess=45)
# Ballad Vocal (10)
for nm, algo, dec, mx in [("Piano Ballad Plate", PLATE, 2.6, 22), ("Gospel Hall", HALL, 3.4, 24), ("Soul Chamber Ballad", CHAMBER, 2.2, 22), ("Big Finale Hall", HALL, 5.0, 24),
                          ("Acoustic Ballad Room", ROOM, 1.2, 18), ("Torch Song Plate", PLATE, 2.2, 22), ("Choir Loft", HALL, 4.0, 26), ("Wedding Hall", HALL, 3.0, 22),
                          ("Lullaby Ambience", AMB, 0.8, 18), ("Shimmer Ballad", SHIM, 4.5, 22)]:
    S("Ballad Vocal | " + nm, algo, dec, mx, pre=35, damp=50)
# Vocal FX (16)
S("Vocal FX | Reverse Swell Hall", HALL, 3.0, 30, rev=1, revt=500)
S("Vocal FX | Reverse Plate Ghost", PLATE, 2.0, 30, rev=1, revt=350)
S("Vocal FX | Octave Down Cave", HALL, 4.0, 26, pitch=-12, damp=70)
S("Vocal FX | Octave Up Angel Verb", HALL, 3.5, 26, pitch=12)
S("Vocal FX | Fifth Harmony Verb", PLATE, 2.5, 26, pitch=7)
S("Vocal FX | Throat Monster Room", ROOM, 1.0, 26, throat=100, pitch=-5)
S("Vocal FX | Tube Radio Verb", SPRING, 1.6, 26, tube=80, hc=4500, lc=400)
S("Vocal FX | Telephone Room", ROOM, 0.8, 24, lc=600, hc=3500, tube=40)
S("Vocal FX | Gated Eighties Verb", HALL, 2.0, 26, gate=65, size=70)
S("Vocal FX | Infinite Freeze Wash", SHIM, 18.0, 28, shim=55, size=100)
S("Vocal FX | Alien Shimmer", SHIM, 6.0, 28, shim=55, shp=2, pitch=5)
S("Vocal FX | Squashed Room", ROOM, 1.0, 24, comp=90)
S("Vocal FX | Underwater Hall", HALL, 3.0, 26, hc=1800, mod=90, rate=2.5)
S("Vocal FX | Seasick Plate", PLATE, 2.0, 26, mod=100, rate=3.0)
S("Vocal FX | Dub Siren Spring", SPRING, 3.5, 28, dmode=2, dsync=11, dfb=65, dlev=40)
S("Vocal FX | Reverse Echo Spring", SPRING, 2.0, 28, rev=1, revt=250, dmode=1, dsync=13, dfb=40)

#=====================================================================================================================
# Mic Closet - mic indices as in MicCloset::mics()
M = {n: i for i, n in enumerate(["Flat (measurement)", "Large-Diaphragm Tube Condenser", "Large-Diaphragm Tube, Dark", "Large-Diaphragm FET Condenser", "Modern Bright Studio Condenser",
    "Vintage Bottle Condenser", "Small-Diaphragm Pencil", "Small-Diaphragm Omni Pencil", "Multi-Pattern Studio Condenser", "Budget Large Condenser", "USB Desktop Condenser",
    "Dynamic Stage Handheld", "Supercardioid Stage Dynamic", "Broadcast End-Address Dynamic", "Broadcast Large Dynamic", "Instrument Dynamic", "Kick Drum Dynamic",
    "Vintage Bullet Dynamic", "Vintage Pill Dynamic", "1950s Ribbon", "1930s Ribbon", "Modern Active Ribbon", "Broadcast Ribbon", "Lavalier", "Headset Boom", "Shotgun",
    "Boundary / Plate", "Smartphone Built-In", "Laptop Built-In", "Webcam", "Telephone Handset", "Two-Way Radio", "Megaphone Horn", "Vintage Carbon Mic", "Tube Talkback Speaker",
    "Crystal Lo-Fi Mic", "Squelchmoth Velvet", "Squelchmoth Glass", "Squelchmoth Smoke", "Squelchmoth Gold", "Squelchmoth Iron", "Squelchmoth Halo", "Squelchmoth Mud Hut",
    "Squelchmoth Night Radio", "Squelchmoth Cathedral Air"])}
MV = ['distance', 'lowCut', 'pad', 'colour', 'noise', 'mix', 'output']
MP = ['source', 'target', 'pattern', 'sourceProxOn']
plug('Mic Closet', MV, MP)
def MC(name, src, tgt, dist=15, lc=20, colour=50, pat=0, pad=0, noise=0, mix=100, sprox=1):
    add('Mic Closet', name, dict(distance=dist, lowCut=lc, pad=pad, colour=colour, noise=noise, mix=mix), dict(source=M[src], target=M[tgt], pattern=pat, sourceProxOn=sprox))
# Vocal Upgrade (20): home and device mics to studio mics
for src in ["USB Desktop Condenser", "Budget Large Condenser", "Dynamic Stage Handheld", "Headset Boom", "Smartphone Built-In"]:
    short = {"USB Desktop Condenser": "USB Mic", "Budget Large Condenser": "Budget Condenser", "Dynamic Stage Handheld": "Stage Dynamic", "Headset Boom": "Headset", "Smartphone Built-In": "Phone"}[src]
    for tgt, tn in [("Large-Diaphragm Tube Condenser", "Tube Condenser"), ("Large-Diaphragm FET Condenser", "FET Condenser"), ("Modern Bright Studio Condenser", "Bright Studio"), ("Squelchmoth Velvet", "Velvet")]:
        MC("Vocal Upgrade | %s to %s" % (short, tn), src, tgt, dist=12, lc=60 if src != "Smartphone Built-In" else 90)
# Vocal Character (20): flat (unknown) source to every vocal flavour
for tgt, dist, colour in [("Large-Diaphragm Tube Condenser", 12, 50), ("Large-Diaphragm Tube, Dark", 12, 60), ("Large-Diaphragm FET Condenser", 15, 40), ("Modern Bright Studio Condenser", 15, 20),
                          ("Vintage Bottle Condenser", 15, 60), ("Multi-Pattern Studio Condenser", 15, 40), ("Small-Diaphragm Pencil", 25, 20), ("1950s Ribbon", 15, 50),
                          ("1930s Ribbon", 20, 60), ("Modern Active Ribbon", 15, 30), ("Dynamic Stage Handheld", 5, 0), ("Supercardioid Stage Dynamic", 5, 0),
                          ("Vintage Pill Dynamic", 10, 40), ("Squelchmoth Velvet", 12, 50), ("Squelchmoth Glass", 15, 20), ("Squelchmoth Smoke", 15, 60),
                          ("Squelchmoth Gold", 12, 55), ("Squelchmoth Iron", 10, 70), ("Squelchmoth Halo", 18, 30), ("Squelchmoth Cathedral Air", 20, 20)]:
    MC("Vocal Character | " + tgt.replace("Large-Diaphragm", "LD").replace("Small-Diaphragm", "SD"), "Flat (measurement)", tgt, dist=dist, colour=colour, lc=40)
# Podcast Voice (12)
for nm, src, tgt, dist in [("Broadcast Warmth", "USB Desktop Condenser", "Broadcast End-Address Dynamic", 5), ("Big Radio Voice", "Dynamic Stage Handheld", "Broadcast Large Dynamic", 4),
                           ("Laptop Rescue", "Laptop Built-In", "Broadcast End-Address Dynamic", 8), ("Webcam Rescue", "Webcam", "Large-Diaphragm FET Condenser", 15),
                           ("Phone Interview Fix", "Smartphone Built-In", "Broadcast End-Address Dynamic", 10), ("Lavalier Body", "Lavalier", "Large-Diaphragm FET Condenser", 15),
                           ("Headset to Studio", "Headset Boom", "Broadcast End-Address Dynamic", 6), ("Late Night Host", "USB Desktop Condenser", "Broadcast Ribbon", 6),
                           ("Audiobook Narrator", "Budget Large Condenser", "Large-Diaphragm Tube Condenser", 20), ("Field Shotgun Smooth", "Shotgun", "Large-Diaphragm FET Condenser", 25),
                           ("Close Talk Intimacy", "Flat (measurement)", "Broadcast Large Dynamic", 3), ("News Desk", "Flat (measurement)", "Broadcast End-Address Dynamic", 8)]:
    MC("Podcast Voice | " + nm, src, tgt, dist=dist, lc=80)
# Lo-Fi Voice (14)
for nm, tgt, colour, noise in [("Telephone Line", "Telephone Handset", 70, 20), ("Walkie Talkie", "Two-Way Radio", 80, 25), ("Megaphone", "Megaphone Horn", 90, 15),
                               ("Old Carbon Mic", "Vintage Carbon Mic", 90, 30), ("Talkback Speaker", "Tube Talkback Speaker", 70, 20), ("Crystal Mic", "Crystal Lo-Fi Mic", 70, 25),
                               ("Night Radio", "Squelchmoth Night Radio", 80, 30), ("Mud Hut", "Squelchmoth Mud Hut", 80, 20), ("Bullet Harp Mic", "Vintage Bullet Dynamic", 80, 15),
                               ("Laptop Voice", "Laptop Built-In", 40, 20), ("Webcam Call", "Webcam", 40, 25), ("Phone Memo", "Smartphone Built-In", 40, 20),
                               ("1930s Broadcast", "1930s Ribbon", 80, 25), ("Lo-Fi Hook", "Crystal Lo-Fi Mic", 60, 10)]:
    MC("Lo-Fi Voice | " + nm, "Flat (measurement)", tgt, colour=colour, noise=noise, dist=10)
# Distance & Pattern (14)
for nm, tgt, dist, pat in [("Kiss The Grille", "Large-Diaphragm Tube Condenser", 2, 1), ("Close Cardioid", "Large-Diaphragm FET Condenser", 5, 1), ("Arm's Length", "Large-Diaphragm FET Condenser", 50, 1),
                           ("Across The Room", "Small-Diaphragm Omni Pencil", 100, 2), ("Omni Natural", "Multi-Pattern Studio Condenser", 20, 2), ("Figure-8 Ribbon Close", "1950s Ribbon", 4, 3),
                           ("Figure-8 Ribbon Far", "1950s Ribbon", 60, 3), ("Supercardioid Live", "Supercardioid Stage Dynamic", 3, 4), ("Proximity Bass Boost", "Dynamic Stage Handheld", 1, 1),
                           ("Thin Distant Voice", "Small-Diaphragm Pencil", 80, 1), ("Choir Pair Omni", "Small-Diaphragm Omni Pencil", 70, 2), ("Close Ribbon Warmth", "Broadcast Ribbon", 3, 0),
                           ("Shotgun Off Axis", "Shotgun", 60, 4), ("Boundary Room", "Boundary / Plate", 80, 2)]:
    MC("Distance & Pattern | " + nm, "Flat (measurement)", tgt, dist=dist, pat=pat, colour=30)

#=====================================================================================================================
# Larynx
LV = ['length', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8', 'glottal', 'pulseWidth', 'breath', 'breathFreq', 'precision', 'mix']
plug('Larynx', LV + ['output'], ['levelMatchOn'])
def LX(name, length=100, shape=(0,) * 8, glottal=0, pw=50, breath=0, bf=3000, prec=50, mix=100):
    v = dict(length=length, glottal=glottal, pulseWidth=pw, breath=breath, breathFreq=bf, precision=prec, mix=mix)
    for i, x in enumerate(shape): v['p%d' % (i + 1)] = x
    add('Larynx', name, v, dict(levelMatchOn=1))
# Subtle Vocal (12)
LX("Subtle Vocal | Slightly Younger", length=94); LX("Subtle Vocal | Slightly Older", length=106); LX("Subtle Vocal | Open Throat", shape=(0, 20, 30, 30, 10, 0, 0, 0))
LX("Subtle Vocal | Forward Placement", shape=(0, 0, 0, -10, -10, 10, 25, 15)); LX("Subtle Vocal | Warmer Chest", length=104, glottal=-20, pw=60); LX("Subtle Vocal | Brighter Edge", glottal=25, pw=42)
LX("Subtle Vocal | Softer Delivery", glottal=-35, pw=62, breath=10); LX("Subtle Vocal | Tighter Delivery", glottal=30, pw=40); LX("Subtle Vocal | Rounder Vowels", shape=(0, 0, 10, 20, 20, 0, -10, 30))
LX("Subtle Vocal | Narrow Nasal Fix", shape=(0, 0, 0, 20, 0, 0, 0, 10)); LX("Subtle Vocal | Smile Tone", shape=(0, 0, 0, 0, 0, 15, 30, -20)); LX("Subtle Vocal | Pillow Breath", breath=15, bf=4500, glottal=-15)
# Bigger Voice (10)
for i, (nm, L, g) in enumerate([("Big Baritone", 118, -10), ("Giant", 140, -20), ("Deep Narrator", 112, 0), ("Bass Choir", 125, -15), ("Movie Trailer", 128, 10),
                                ("Gentle Giant", 135, -40), ("Barrel Chest", 115, 20), ("Ogre", 155, 30), ("Wide Throat Tenor", 108, 0), ("Old Sage", 120, -30)]):
    LX("Bigger Voice | " + nm, length=L, glottal=g, shape=(0, 20, 30, 20, 0, 0, 0, 10) if i % 2 else (0,) * 8, breath=20 if 'Sage' in nm else 0)
# Smaller Voice (10)
for i, (nm, L, g) in enumerate([("Youthful", 88, 10), ("Child-Like", 75, 15), ("Pixie", 68, 25), ("Small Bright Soprano", 84, 20), ("Cartoon Mouse", 62, 30),
                                ("Sprite Whisper", 72, -30), ("Tiny Robot Helper", 66, 60), ("Light Alto", 92, 0), ("Elf", 78, 10), ("Fairy Choir", 70, -10)]):
    LX("Smaller Voice | " + nm, length=L, glottal=g, breath=40 if 'Whisper' in nm else 0, shape=(0, -10, -10, 0, 0, 10, 20, 0) if i % 2 else (0,) * 8)
# Character (10)
LX("Character | Nasal Twang", shape=(0, 0, -30, 60, -20, 0, 0, -30)); LX("Character | Covered Opera", length=108, shape=(0, 30, 50, 40, 0, 0, -10, 40), glottal=-10)
LX("Character | Pinched Comic", shape=(0, -40, -50, -20, 0, 20, 30, -40), glottal=40); LX("Character | Hollow Ghost", shape=(0, 40, 60, 60, 40, 0, 0, 50), breath=35, glottal=-40)
LX("Character | Gravel Growl", length=115, glottal=80, pw=25); LX("Character | Soft Crooner", length=106, glottal=-50, pw=70, breath=15)
LX("Character | Mouth Closed Hum", shape=(0, 0, 0, 0, 0, 0, -30, -80)); LX("Character | Wide Open Shout", shape=(0, 30, 40, 30, 20, 30, 40, 70), glottal=50)
LX("Character | Cowboy Drawl", length=110, shape=(0, 0, 0, 30, 20, 0, -20, -10)); LX("Character | Valley Fry", glottal=70, pw=20, length=96)
# Breathy (8)
for nm, b, bf, g in [("Breathy Pop", 30, 4000, -20), ("Whisper Lead", 70, 3500, -60), ("Airy Falsetto", 45, 5500, -40), ("Husky Alto", 35, 2500, -10), ("Sultry", 40, 3000, -35),
                     ("Exhausted", 55, 2200, -50), ("Steam Breath", 80, 6500, -70), ("ASMR Close", 50, 7000, -50)]:
    LX("Breathy | " + nm, breath=b, bf=bf, glottal=g, pw=65)
# Creature FX (10)
for nm, L, sh, g, b in [("Alien Envoy", 70, (60, -60, 60, -60, 60, -60, 60, -60), 40, 0), ("Swamp Beast", 160, (0, 60, -40, 60, -40, 60, 0, 0), 80, 20),
                        ("Insect Queen", 60, (-60, -60, 40, 40, -60, -60, 40, 40), 70, 10), ("Robot Throat", 100, (80, -80, 80, -80, 80, -80, 80, -80), 90, 0),
                        ("Wraith", 130, (0, 50, 70, 70, 50, 0, 0, 60), -80, 70), ("Goblin", 74, (0, -30, -50, 40, 0, 30, -40, -50), 60, 0),
                        ("Deep Sea Voice", 150, (0, 70, 80, 60, 40, 20, 0, -40), -20, 30), ("Wind Spirit", 85, (0, 20, 20, 20, 20, 20, 20, 80), -90, 90),
                        ("Lizard King", 120, (0, -50, 30, -50, 30, -50, 30, -50), 70, 15), ("Glass Golem", 90, (50, 50, -60, -60, 50, 50, -60, -60), 50, 0)]:
    LX("Creature FX | " + nm, length=L, shape=sh, glottal=g, breath=b, prec=80)

#=====================================================================================================================
# Exhale
EV = ['breath', 'tone', 'consonants', 'body', 'mix']
plug('Exhale', EV + ['output'], [])
def EX(name, breath, tone=0, cons=70, body=0, mix=100): add('Exhale', name, dict(breath=breath, tone=tone, consonants=cons, body=body, mix=mix), {})
for nm, b, cons in [("Remove All Breath", -60, 80), ("Remove Breath Keep Consonants", -60, 100), ("Deep Breath Cut", -24, 70), ("Strong Reduce", -15, 70), ("Medium Reduce", -9, 70),
                    ("Light Reduce", -5, 70), ("Touch Of Less", -3, 70)]:
    EX("Breath Removal | " + nm, b, cons=cons)
for nm, b, t in [("Clean Radio Voice", -12, 0), ("Podcast Polish", -8, -20), ("Audiobook Smooth", -10, -30), ("Dark Breath Tame", -6, -60), ("Hiss Tame", -6, -80)]:
    EX("Spoken Voice | " + nm, b, tone=t)
for nm, b, t in [("Airy Pop Lift", 4, 30), ("Intimate Breath", 6, 10), ("Bright Air", 5, 60), ("Breathy R&B", 7, 20), ("Falsetto Air", 8, 40), ("Silky Lift", 3, 20)]:
    EX("Airy Lift | " + nm, b, tone=t)
for nm, b, t, body in [("Whisper Mode", 12, 20, -9), ("Half Whisper", 10, 10, -5), ("Breath Pad", 12, 0, -12), ("Ghost Voice", 12, 40, -10)]:
    EX("Whisper FX | " + nm, b, tone=t, body=body, cons=100)
for nm, b, t, cons in [("Soft Consonants", -4, -30, 40), ("Sibilance Friendly", -3, -50, 20), ("Crisp Consonants", 0, 30, 100), ("Rap Clarity", -6, 20, 100),
                       ("Choir Smooth", -9, -20, 60), ("Backing Vocal Clean", -12, 0, 80), ("Live Mic Breath Fix", -9, -30, 70), ("Ballad Breath Control", -6, 0, 70)]:
    EX("Consonant Care | " + nm, b, tone=t, cons=cons)

#=====================================================================================================================
WANT = {'Siren Verb': 140, 'Mic Closet': 80, 'Larynx': 60, 'Exhale': 30}
CPP = {'Siren Verb': 'sverb', 'Mic Closet': 'mic', 'Larynx': 'larynx', 'Exhale': 'exhale'}
CLS = {'Siren Verb': 'SqSirenVerbPlugin', 'Mic Closet': 'SqMicClosetPlugin', 'Larynx': 'SqLarynxPlugin', 'Exhale': 'SqExhalePlugin'}

def specs_defaults():
    src = open(os.path.join(ROOT, 'Source', 'SqVoice111e.h')).read()
    d = {}
    for nm, cls in CLS.items():
        body = src[src.index('class ' + cls):]
        body = body[:body.index('return s;')]
        d[nm] = {m.group(1): float(m.group(2).rstrip('f')) for m in re.finditer(r'\{ "(\w+)",\s*"[^"]+",\s*[-\d.f]+,\s*[-\d.f]+,\s*([-\d.f]+),', body)}
    return d

def write_table(path, defs):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111g.py - do not edit.\n#pragma once\n')
        f.write('struct E1Row { const char* name; float v[32]; int pr[16]; };\n')
        for nm, (params, props, lst) in PLUGS.items():
            allp = list(defs[nm].keys())
            f.write('// %s values: %s | props: %s\n' % (nm, ' '.join(allp), ' '.join(props)))
            f.write('static const E1Row %sRows[] = {\n' % CPP[nm])
            for p in lst:
                v = [p['vals'].get(k, defs[nm][k]) for k in allp]
                pr = [int(p['props'].get(k, 0)) for k in props] or [0]
                f.write('    { %s, { %s }, { %s } },\n' % (json.dumps(p['name']), ', '.join('%g' % x for x in v), ', '.join(str(x) for x in pr)))
            f.write('};\n')

def calibrate():
    exe = subprocess.check_output(['bash', os.path.join(ROOT, 'scripts', 'build_voice111e_test.sh'), os.path.join(ROOT, 'tests', 'preset_calib_e1.cpp'), 'preset_calib_e1'], text=True).strip().splitlines()[-1]
    out = subprocess.run([exe], text=True, capture_output=True, check=True)
    open('/tmp/calib_e1.log', 'w').write(out.stderr)
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
    for nm, (params, props, lst) in PLUGS.items():
        names = [p['name'] for p in lst]
        dup = set(n for n in names if names.count(n) > 1)
        assert not dup, (nm, dup)
        cats = sorted(set(n.split(' | ')[0] for n in names))
        print('%s: %d presets (want %d) in %d categories' % (nm, len(lst), WANT[nm], len(cats)))
        assert len(lst) == WANT[nm], nm
    defs = specs_defaults()
    write_table(os.path.join(ROOT, 'tests', 'presets111g_table.h'), defs)
    trims = calibrate()
    fns, adds = [], []
    worst = 0
    for k, (nm, (params, props, lst)) in enumerate(PLUGS.items()):
        lines = []
        for p in lst:
            trim, peak = trims[(nm, p['name'])]
            trim = min(trim, 3.0 - peak)
            trim = max(-24.0, min(12.0, trim)); worst = max(worst, abs(trim))
            vals = ', '.join('N (%s, %s)' % (json.dumps(kk), num(vv)) for kk, vv in p['vals'].items()) + ', N ("output", %s)' % num(round(trim, 2))
            prs = ', '.join('{ "%s", %d }' % (kk, int(vv)) for kk, vv in p['props'].items())
            lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111g_%d()   // %s\n{\n    return {\n%s\n    };\n}\n' % (k + 1, nm, '\n'.join(lines)))
        adds.append('    m[%s] = presets111g_%d();\n' % (json.dumps(nm), k + 1))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111g.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111g.py - factory presets for the 1.11 Voice second wave (part 1). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111f.inl; bank() calls add111g().\n')
        f.write('// Output = the measured trim that holds the level on the sung vocal.\n\n')
        f.write('\n'.join(fns))
        f.write('inline void add111g (std::map<juce::String, std::vector<Preset>>& m)\n{\n' + ''.join(adds) + '}\n')
    print('wrote', out, '- largest trim %.2f dB' % worst)

if __name__ == '__main__':
    main()
