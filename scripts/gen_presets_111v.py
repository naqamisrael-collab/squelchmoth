#!/usr/bin/env python3
"""Factory presets for the 1.11 Voice family, first wave (super prompt section 3, phase C):
   Moth Tune 2 (150), Siren Comp (110), Siren EQ (100), Hush De-Ess 2 (60).

Siren Comp, Siren EQ and Hush De-Ess 2 presets are rendered through the real DSP (tests/preset_calib_voice.cpp, built
from Source/SqVoice111Dsp.h) and their Output trim is written from the measurement, so every preset holds its level on
the material it was made for (the sung vocal; instrument presets on pink noise). Moth Tune presets correct pitch and
keep level; presets with harmony voices get an Output trim for the voices they add.

Writes tests/presets111v_table.h and Source/SignaturePresets111v.inl (add111v(), called from SignaturePresets::bank()).
"""
import json, os, re, subprocess, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

#=====================================================================================================================
# Siren Comp. model: 0 FET, 1 Opto A, 2 Opto B, 3 Modern. fet: 0 4:1, 1 8:1, 2 12:1, 3 20:1, 4 All.
COMP = []
def C(name, m1, thr1, atk1=5, rel1=150, ratio1=4, knee=6, fet1=0, dual=False, m2=1, thr2=-24, atk2=10, rel2=300, ratio2=3, fet2=0,
      warm=0, mix=100, punch=False, pgain=0, impact=0, ceil=-1, duck=6):
    if not (name.startswith('Parallel') or name.startswith('Creative')):
        thr1 += 6; thr2 += 6            # voiced against a -18 dBFS RMS vocal: 3-8 dB of reduction on the loud words
    elif name.startswith('Creative') and not 'Punch' in name:
        thr1 += 14; thr2 += 14          # still extreme, but never 20 dB quieter than the input
    COMP.append(dict(name=name, m1=m1, fet1=fet1, thr1=thr1, atk1=atk1, rel1=rel1, ratio1=ratio1, knee=knee, dual=dual, m2=m2, fet2=fet2,
                     thr2=thr2, atk2=atk2, rel2=rel2, ratio2=ratio2, warm=warm, mix=mix, punch=punch, pgain=pgain, impact=impact, ceil=ceil, duck=duck))
FET, OA, OB, MOD = 0, 1, 2, 3
# Pop Vocal (14)
C("Pop Vocal | Radio Ready Lead", FET, -24, 0.3, 120, fet1=0, dual=True, m2=OA, thr2=-26, warm=15)
C("Pop Vocal | Glossy Chorus Lead", FET, -26, 0.2, 90, fet1=1, dual=True, m2=OA, thr2=-28, warm=10)
C("Pop Vocal | Smooth Opto Lead", OA, -28, warm=8)
C("Pop Vocal | Upfront Verse", MOD, -24, 3, 120, 4, 6, warm=10)
C("Pop Vocal | Tight Modern Pop", MOD, -26, 1.5, 80, 5, 4, dual=True, m2=OA, thr2=-26)
C("Pop Vocal | Breathy Intimate", OA, -32, warm=5)
C("Pop Vocal | Big Ballad Lead", OB, -24, dual=False, warm=12)
C("Pop Vocal | Bright Pop Punch", FET, -22, 0.15, 70, fet1=1, punch=True, pgain=2, impact=25, ceil=-1)
C("Pop Vocal | Even Every Word", MOD, -30, 4, 160, 3, 10, dual=True, m2=OA, thr2=-30)
C("Pop Vocal | Light Touch", OA, -22)
C("Pop Vocal | Dense Hook", FET, -28, 0.1, 60, fet1=2, dual=True, m2=OB, thr2=-26, warm=15)
C("Pop Vocal | Gentle Glue", MOD, -22, 10, 250, 2, 12)
C("Pop Vocal | Two Stage Polish", FET, -24, 0.4, 100, fet1=0, dual=True, m2=OA, thr2=-30, warm=10)
C("Pop Vocal | Parallel Pop Lift", FET, -34, 0.1, 50, fet1=4, mix=35, warm=20)
# R&B Vocal (11)
C("R&B Vocal | Silky Opto", OA, -30, warm=12)
C("R&B Vocal | Smooth Runs", MOD, -28, 6, 200, 3, 10, dual=True, m2=OA, thr2=-28)
C("R&B Vocal | Warm Neo Soul", OB, -26, warm=25)
C("R&B Vocal | Modern R&B Lead", FET, -26, 0.3, 110, fet1=0, dual=True, m2=OA, thr2=-28, warm=10)
C("R&B Vocal | Falsetto Control", OA, -32, warm=6)
C("R&B Vocal | Late Night Whisper", MOD, -34, 8, 220, 3, 12, warm=10)
C("R&B Vocal | Belting Tamer", FET, -22, 0.2, 90, fet1=1, dual=True, m2=OB, thr2=-24)
C("R&B Vocal | Velvet Leveler", OA, -27, dual=True, m2=OA, thr2=-30, warm=15)
C("R&B Vocal | Ad-Lib Sit Back", MOD, -30, 2, 120, 4, 6, mix=80)
C("R&B Vocal | Slow Jam Glue", OB, -28, warm=20)
C("R&B Vocal | Crisp R&B Punch", FET, -24, 0.15, 80, fet1=0, punch=True, pgain=1.5, impact=30)
# Rap Vocal (13)
C("Rap Vocal | Upfront Rap Lead", FET, -26, 0.1, 60, fet1=1, dual=True, m2=OA, thr2=-26, warm=10)
C("Rap Vocal | Trap Lead Control", FET, -28, 0.08, 50, fet1=2, dual=True, m2=OA, thr2=-26)
C("Rap Vocal | Boom Bap Bark", FET, -24, 0.2, 80, fet1=3, warm=25)
C("Rap Vocal | Melodic Rap", FET, -24, 0.3, 100, fet1=0, dual=True, m2=OA, thr2=-28, warm=10)
C("Rap Vocal | Aggressive Drill", FET, -30, 0.05, 40, fet1=4, dual=True, m2=MOD, thr2=-24, warm=20)
C("Rap Vocal | Double Track Glue", MOD, -28, 3, 90, 4, 6)
C("Rap Vocal | Punchy Bars", FET, -24, 0.12, 70, fet1=1, punch=True, pgain=3, impact=40)
C("Rap Vocal | Laid Back Flow", OA, -28, warm=10)
C("Rap Vocal | Grime Snap", FET, -26, 0.06, 45, fet1=2, warm=15)
C("Rap Vocal | Wide Dynamic Verse", MOD, -24, 5, 140, 3, 8)
C("Rap Vocal | Ad-Lib Squeeze", FET, -32, 0.05, 40, fet1=4, mix=70)
C("Rap Vocal | Studio Polish Rap", FET, -26, 0.25, 90, fet1=0, dual=True, m2=OB, thr2=-26, warm=8)
C("Rap Vocal | Hype Man Energy", FET, -30, 0.08, 50, fet1=3, punch=True, pgain=2, impact=50)
# Rock Vocal (9)
C("Rock Vocal | Rock Lead FET", FET, -24, 0.2, 90, fet1=1, warm=20)
C("Rock Vocal | Screamer Control", FET, -28, 0.05, 50, fet1=4, dual=True, m2=OB, thr2=-24, warm=25)
C("Rock Vocal | Classic Rock Opto", OB, -24, warm=20)
C("Rock Vocal | Indie Lead", OA, -24, warm=15)
C("Rock Vocal | Punk Shout", FET, -30, 0.05, 40, fet1=4, warm=35)
C("Rock Vocal | Arena Chorus", FET, -26, 0.15, 80, fet1=2, dual=True, m2=OA, thr2=-26, warm=15)
C("Rock Vocal | Garage Grit", FET, -26, 0.1, 60, fet1=3, warm=45)
C("Rock Vocal | Alt Rock Even", MOD, -26, 3, 120, 4, 6, dual=True, m2=OA, thr2=-26)
C("Rock Vocal | Metal Lead Wall", FET, -30, 0.05, 45, fet1=3, dual=True, m2=MOD, thr2=-22, warm=20)
# Country & Folk Vocal (7)
C("Country Vocal | Nashville Lead", OA, -26, warm=10, dual=True, m2=MOD, thr2=-24)
C("Country Vocal | Front Porch Folk", OA, -24, warm=8)
C("Country Vocal | Modern Country Pop", FET, -24, 0.3, 110, fet1=0, dual=True, m2=OA, thr2=-26)
C("Country Vocal | Bluegrass High Lead", MOD, -24, 4, 150, 3, 8)
C("Country Vocal | Singer Songwriter", OA, -26, warm=6)
C("Country Vocal | Americana Warmth", OB, -24, warm=22)
C("Country Vocal | Harmony Blend", MOD, -28, 5, 180, 3, 10)
# Gospel & Soul Vocal (7)
C("Gospel Vocal | Powerhouse Lead", FET, -22, 0.2, 100, fet1=1, dual=True, m2=OB, thr2=-22, warm=12)
C("Gospel Vocal | Choir Glue", MOD, -26, 8, 220, 2.5, 12)
C("Gospel Vocal | Soul Shout", FET, -26, 0.1, 70, fet1=2, warm=20)
C("Gospel Vocal | Praise Leader", OA, -24, dual=True, m2=MOD, thr2=-22)
C("Gospel Vocal | Runs and Riffs", MOD, -26, 2, 100, 4, 6)
C("Gospel Vocal | Vintage Soul", OB, -24, warm=30)
C("Gospel Vocal | Big Room Dynamics", OA, -22, warm=10)
# Backing Vocal (8)
C("Backing Vocal | Stack Glue", MOD, -30, 5, 160, 4, 8)
C("Backing Vocal | Tucked Doubles", FET, -30, 0.2, 90, fet1=1, warm=5)
C("Backing Vocal | Ooh Pad Smooth", OB, -30)
C("Backing Vocal | Gang Vocal", FET, -32, 0.08, 60, fet1=3, warm=20)
C("Backing Vocal | Harmony Bus", MOD, -28, 10, 250, 3, 12)
C("Backing Vocal | Airy Stack", OA, -30, warm=5)
C("Backing Vocal | Chant Squeeze", FET, -30, 0.1, 60, fet1=4, mix=60)
C("Backing Vocal | Choir Leveler", OA, -28, dual=True, m2=OA, thr2=-32)
# Voice-Over & Podcast (9)
C("Voice-Over | Broadcast Voice", FET, -26, 0.3, 110, fet1=1, dual=True, m2=OB, thr2=-24, warm=10)
C("Voice-Over | Podcast Clean", MOD, -26, 5, 160, 3, 8, dual=True, m2=OA, thr2=-28)
C("Voice-Over | Audiobook Even", OA, -28, dual=True, m2=MOD, thr2=-26)
C("Voice-Over | Trailer Voice", FET, -28, 0.1, 80, fet1=2, dual=True, m2=OB, thr2=-24, warm=20)
C("Voice-Over | Interview Leveler", OB, -30)
C("Voice-Over | YouTube Presence", FET, -24, 0.2, 90, fet1=0, punch=True, pgain=2, impact=20)
C("Voice-Over | Radio DJ", FET, -30, 0.08, 60, fet1=3, dual=True, m2=OB, thr2=-22, warm=15)
C("Voice-Over | Gentle Narrator", OA, -24)
C("Voice-Over | Phone Call Clarity", MOD, -28, 3, 120, 4, 6)
# Live Vocal (6)
C("Live Vocal | Stage Lead Safe", FET, -22, 0.3, 120, fet1=0)
C("Live Vocal | In-Ear Mix", OA, -26)
C("Live Vocal | Loud Band Lead", FET, -26, 0.1, 80, fet1=2, warm=10)
C("Live Vocal | Worship Stage", MOD, -24, 4, 150, 3, 8, dual=True, m2=OA, thr2=-26)
C("Live Vocal | Club PA Vocal", FET, -24, 0.15, 90, fet1=1, punch=True, pgain=1, impact=20)
C("Live Vocal | Acoustic Set", OA, -24, warm=8)
# Vintage Vocal (7)
C("Vintage Vocal | Sixties Opto", OB, -26, warm=40)
C("Vintage Vocal | Motor City Squeeze", FET, -24, 0.3, 120, fet1=1, warm=35)
C("Vintage Vocal | Seventies Rock", FET, -26, 0.2, 100, fet1=2, dual=True, m2=OB, thr2=-24, warm=30)
C("Vintage Vocal | Crooner", OA, -26, warm=30)
C("Vintage Vocal | Tube Leveler", OB, -28, warm=45)
C("Vintage Vocal | Doo-Wop Group", MOD, -26, 6, 200, 3, 12, warm=30)
C("Vintage Vocal | Eighties Power", FET, -26, 0.15, 80, fet1=1, dual=True, m2=OA, thr2=-26, warm=20)
# Parallel Vocal (6)
C("Parallel Vocal | NY Vocal 30%", FET, -36, 0.05, 50, fet1=4, mix=30, warm=20)
C("Parallel Vocal | Smash Blend 20%", FET, -40, 0.05, 40, fet1=4, mix=20, warm=30)
C("Parallel Vocal | Opto Thicken 40%", OB, -36, mix=40, warm=20)
C("Parallel Vocal | Rap Density 35%", FET, -38, 0.05, 45, fet1=3, mix=35, warm=15)
C("Parallel Vocal | Ballad Body 25%", OB, -38, mix=25, warm=10)
C("Parallel Vocal | Breath Lift 30%", MOD, -42, 0.5, 80, 10, 4, mix=30)
# Creative (6) - level exempt by design
C("Creative | Pumping Vocal FX", FET, -40, 0.05, 300, fet1=4, warm=40)
C("Creative | Lo-Fi Squash", FET, -38, 0.05, 50, fet1=4, warm=80)
C("Creative | Breath Monster", MOD, -45, 0.2, 400, 20, 0)
C("Creative | Telephone Crush", FET, -36, 0.05, 40, fet1=3, warm=60)
C("Creative | Infinite Sustain", OB, -45, warm=30)
C("Creative | Punch Clipper", MOD, -20, 2, 100, 2, 6, punch=True, pgain=12, impact=60, ceil=-3)
# Instrument (7) - pink noise target
C("Instrument | Acoustic Guitar", OA, -26)
C("Instrument | Bass DI Lock", FET, -24, 0.4, 150, fet1=1)
C("Instrument | Piano Even", MOD, -24, 10, 200, 2.5, 10)
C("Instrument | Snare Snap", FET, -22, 0.8, 80, fet1=1, punch=True, pgain=2, impact=50)
C("Instrument | Horn Section", OB, -24, warm=15)
C("Instrument | Synth Lead Tame", MOD, -24, 3, 120, 4, 6)
C("Instrument | Strings Glue", OA, -26)

#=====================================================================================================================
# Siren EQ. type: 0 bell, 1 low shelf, 2 high shelf, 3 notch. mode: 0 fixed, 1 pitch track, 2 dynamic.
EQ = []
def band(on=True, typ=0, mode=0, harm=1, freq=1000, gain=0, q=1.0, thr=-30, rng=6, expand=False):
    return dict(on=on, type=typ, mode=mode, harm=harm, freq=freq, gain=gain, q=q, thr=thr, range=rng, expand=expand)
def E(name, bands, hp=20, hps=1, lp=20000, lps=1, airf=12000, air=0, tilt=0):
    bs = list(bands) + [band(on=False)] * (6 - len(bands))
    EQ.append(dict(name=name, bands=bs[:6], hp=hp, hps=hps, lp=lp, lps=lps, airf=airf, air=air, tilt=tilt))
LS, HS, NOTCH = 1, 2, 3
PT, DYN = 1, 2
# Female Vocal (12)
E("Female Vocal | Pop Shine", [band(freq=250, gain=-2, q=1.2), band(freq=3200, gain=1.5, q=0.9), band(typ=HS, freq=9000, gain=2, q=0.7)], hp=90, air=2, airf=14000)
E("Female Vocal | Air and Presence", [band(freq=320, gain=-1.5, q=1.0), band(freq=5000, gain=2, q=1.2)], hp=100, air=4, airf=16000)
E("Female Vocal | De-Box Lead", [band(freq=450, gain=-3, q=1.6), band(freq=900, gain=-1.5, q=2)], hp=90)
E("Female Vocal | Warm Body", [band(typ=LS, freq=220, gain=1.5, q=0.7), band(freq=2500, gain=-1, q=1.5)], hp=80, tilt=-1)
E("Female Vocal | Harsh Tamer Dynamic", [band(mode=DYN, freq=3500, gain=0, q=2.5, thr=-34, rng=5), band(mode=DYN, freq=6500, gain=0, q=3, thr=-36, rng=4)], hp=90)
E("Female Vocal | Pitch-Locked Nasal Cut", [band(mode=PT, harm=3, gain=-2.5, q=4), band(freq=1100, gain=-1.5, q=2)], hp=100)
E("Female Vocal | Belter Control", [band(mode=DYN, freq=2800, q=1.8, thr=-30, rng=6), band(typ=HS, freq=10000, gain=1.5, q=0.7)], hp=110)
E("Female Vocal | Soft Ballad", [band(freq=300, gain=-1, q=1), band(freq=4500, gain=1, q=0.8)], hp=80, air=1.5, tilt=0.5)
E("Female Vocal | Breathy Intimate", [band(freq=200, gain=-2, q=1), band(freq=7000, gain=2, q=1)], hp=110, air=3, airf=18000)
E("Female Vocal | Bright Modern", [band(freq=350, gain=-2, q=1.4), band(freq=3000, gain=2, q=1), band(typ=HS, freq=8000, gain=2.5, q=0.7)], hp=110, hps=2)
E("Female Vocal | Fundamental Weight", [band(mode=PT, harm=1, gain=1.5, q=2), band(freq=600, gain=-1.5, q=1.5)], hp=90)
E("Female Vocal | Clean Up", [band(freq=180, gain=-2, q=1), band(freq=1000, gain=-1, q=1.4), band(freq=4000, gain=1, q=1)], hp=120, hps=3)
# Male Vocal (12)
E("Male Vocal | Pop Clarity", [band(freq=200, gain=-2, q=1.2), band(freq=3500, gain=1.5, q=1), band(typ=HS, freq=10000, gain=1.5, q=0.7)], hp=70)
E("Male Vocal | Chest Warmth", [band(typ=LS, freq=180, gain=2, q=0.7), band(freq=400, gain=-1.5, q=1.4)], hp=60)
E("Male Vocal | Mud Remover", [band(freq=250, gain=-3, q=1.4), band(freq=500, gain=-1.5, q=1.6)], hp=80)
E("Male Vocal | Radio Presence", [band(freq=300, gain=-1.5, q=1), band(freq=2800, gain=2.5, q=1.2)], hp=90, air=1.5)
E("Male Vocal | Baritone Body", [band(mode=PT, harm=1, gain=2, q=2), band(freq=350, gain=-1.5, q=1.2)], hp=55)
E("Male Vocal | Nasal Fix", [band(freq=1000, gain=-3, q=2.5), band(mode=PT, harm=4, gain=-1.5, q=4)], hp=80)
E("Male Vocal | Harsh Tamer Dynamic", [band(mode=DYN, freq=3200, q=2, thr=-32, rng=5), band(mode=DYN, freq=5500, q=2.5, thr=-34, rng=4)], hp=80)
E("Male Vocal | Rock Grit", [band(freq=200, gain=-2, q=1), band(freq=1600, gain=2, q=1.2), band(freq=4000, gain=1.5, q=1)], hp=100)
E("Male Vocal | Crooner Velvet", [band(typ=LS, freq=150, gain=1.5, q=0.7), band(freq=3000, gain=-1, q=1.2)], hp=60, tilt=-1)
E("Male Vocal | Tenor Lift", [band(freq=280, gain=-2, q=1.2), band(freq=4500, gain=2, q=1)], hp=90, air=2)
E("Male Vocal | Proximity Fix", [band(typ=LS, freq=200, gain=-3, q=0.7), band(mode=DYN, freq=150, q=1, thr=-24, rng=6)], hp=60)
E("Male Vocal | Polished Lead", [band(freq=220, gain=-1.5, q=1.2), band(freq=2500, gain=1, q=1), band(typ=HS, freq=9000, gain=2, q=0.7)], hp=80, air=1.5)
# Rap Vocal (10)
E("Rap Vocal | Upfront Bars", [band(freq=200, gain=-2, q=1), band(freq=3000, gain=2.5, q=1.2)], hp=100)
E("Rap Vocal | Trap Clarity", [band(freq=300, gain=-2.5, q=1.4), band(freq=5000, gain=2, q=1), band(typ=HS, freq=10000, gain=2, q=0.7)], hp=120, hps=2)
E("Rap Vocal | Boom Bap Chest", [band(typ=LS, freq=150, gain=2, q=0.7), band(freq=1800, gain=1.5, q=1.2)], hp=70)
E("Rap Vocal | Ad-Lib Thin", [band(freq=400, gain=-3, q=1), band(freq=2500, gain=2, q=1)], hp=250, hps=2)
E("Rap Vocal | Dynamic Mud Control", [band(mode=DYN, freq=280, q=1.4, thr=-26, rng=6), band(freq=4000, gain=1.5, q=1)], hp=90)
E("Rap Vocal | Drill Edge", [band(freq=250, gain=-2, q=1.2), band(freq=3500, gain=3, q=1.4)], hp=110, air=2)
E("Rap Vocal | Melodic Rap Air", [band(freq=300, gain=-1.5, q=1.2), band(freq=6000, gain=2, q=1)], hp=100, air=3)
E("Rap Vocal | Double Tuck", [band(freq=200, gain=-3, q=1), band(freq=3000, gain=-1, q=1.4)], hp=150, lp=12000)
E("Rap Vocal | Pitch-Locked Body", [band(mode=PT, harm=1, gain=2, q=2.5), band(mode=PT, harm=2, gain=1, q=3)], hp=70)
E("Rap Vocal | Hard Sibilance Dynamic", [band(mode=DYN, freq=7000, q=3, thr=-36, rng=6)], hp=100)
# R&B Vocal (8)
E("R&B Vocal | Silky Top", [band(freq=280, gain=-1.5, q=1.2), band(typ=HS, freq=8000, gain=2.5, q=0.7)], hp=90, air=2.5)
E("R&B Vocal | Warm Neo Soul", [band(typ=LS, freq=200, gain=1.5, q=0.7), band(freq=2800, gain=-1, q=1.2)], hp=70, tilt=-1)
E("R&B Vocal | Falsetto Air", [band(freq=500, gain=-2, q=1.2), band(freq=9000, gain=2, q=0.8)], hp=140, air=3, airf=18000)
E("R&B Vocal | Runs Smoothing Dynamic", [band(mode=DYN, freq=2500, q=1.6, thr=-30, rng=4), band(mode=DYN, freq=4500, q=2, thr=-32, rng=4)], hp=90)
E("R&B Vocal | Pitch-Locked Shine", [band(mode=PT, harm=6, gain=1.5, q=3), band(freq=300, gain=-1.5, q=1.2)], hp=90)
E("R&B Vocal | Late Night", [band(freq=350, gain=-2, q=1.2), band(freq=3500, gain=-1, q=1.2)], hp=100, lp=14000, tilt=-1)
E("R&B Vocal | Modern R&B Gloss", [band(freq=250, gain=-2, q=1.2), band(freq=3000, gain=1.5, q=1), band(typ=HS, freq=11000, gain=2.5, q=0.7)], hp=100, hps=2)
E("R&B Vocal | Background Silk", [band(freq=400, gain=-2.5, q=1), band(typ=HS, freq=7000, gain=1.5, q=0.7)], hp=180, hps=2)
# Rock Vocal (6)
E("Rock Vocal | Cut Through", [band(freq=250, gain=-2, q=1), band(freq=2000, gain=2.5, q=1.2)], hp=110)
E("Rock Vocal | Grunge Mid Push", [band(freq=900, gain=2, q=1), band(freq=4000, gain=-1, q=1.4)], hp=120)
E("Rock Vocal | Scream Harsh Tamer", [band(mode=DYN, freq=2800, q=1.6, thr=-28, rng=6), band(mode=DYN, freq=5000, q=2, thr=-30, rng=5)], hp=120)
E("Rock Vocal | Classic Rock Lead", [band(freq=300, gain=-1.5, q=1.2), band(freq=3000, gain=2, q=1)], hp=100, lp=16000)
E("Rock Vocal | Indie Thin", [band(freq=200, gain=-3, q=0.8), band(freq=1500, gain=1.5, q=1.2)], hp=200, hps=2)
E("Rock Vocal | Garage Honk", [band(freq=1000, gain=3, q=1.5), band(freq=300, gain=-2, q=1)], hp=150, lp=10000)
# Voice-Over (9)
E("Voice-Over | Broadcast Voice", [band(typ=LS, freq=120, gain=2, q=0.7), band(freq=350, gain=-2, q=1.2), band(freq=3000, gain=1.5, q=1)], hp=60)
E("Voice-Over | Podcast Clean", [band(freq=250, gain=-2, q=1.2), band(freq=3500, gain=1.5, q=1)], hp=80, hps=2)
E("Voice-Over | Audiobook Natural", [band(freq=300, gain=-1, q=1), band(freq=4000, gain=0.5, q=1)], hp=70)
E("Voice-Over | Trailer Depth", [band(typ=LS, freq=100, gain=3, q=0.7), band(freq=400, gain=-2, q=1.2), band(freq=2500, gain=2, q=1)], hp=40)
E("Voice-Over | Room Boom Fix Dynamic", [band(mode=DYN, freq=180, q=1.4, thr=-28, rng=6), band(freq=500, gain=-1.5, q=1.4)], hp=80)
E("Voice-Over | Laptop Mic Rescue", [band(freq=600, gain=-3, q=1.4), band(freq=2500, gain=2, q=1), band(typ=HS, freq=8000, gain=2, q=0.7)], hp=120, hps=2)
E("Voice-Over | Phone Interview", [band(freq=300, gain=-2, q=1), band(freq=2000, gain=2, q=1)], hp=150, lp=9000)
E("Voice-Over | Pitch-Locked Warmth", [band(mode=PT, harm=1, gain=2, q=2), band(mode=PT, harm=2, gain=1, q=2.5)], hp=60)
E("Voice-Over | Presence Dynamic", [band(mode=DYN, freq=3000, gain=0, q=1.2, thr=-40, rng=4, expand=True)], hp=70)
# Backing Vocal (7)
E("Backing Vocal | Tuck Behind Lead", [band(freq=3000, gain=-2.5, q=1), band(freq=250, gain=-2, q=1)], hp=150, hps=2, air=1)
E("Backing Vocal | Airy Stack", [band(freq=400, gain=-2.5, q=1), band(typ=HS, freq=9000, gain=2, q=0.7)], hp=180, hps=2, air=2)
E("Backing Vocal | Choir Blend", [band(freq=300, gain=-1.5, q=1), band(freq=2500, gain=-1.5, q=1.2)], hp=120)
E("Backing Vocal | Thin Doubles", [band(freq=200, gain=-4, q=0.8)], hp=250, hps=3)
E("Backing Vocal | Dark Pad", [band(freq=500, gain=-1, q=1)], hp=150, lp=8000, lps=2, tilt=-2)
E("Backing Vocal | Pitch-Locked Smooth", [band(mode=PT, harm=3, gain=-2, q=3), band(mode=PT, harm=5, gain=-1.5, q=3)], hp=140)
E("Backing Vocal | Gang Shout", [band(freq=800, gain=2, q=1), band(freq=250, gain=-2, q=1)], hp=180, lp=12000)
# Problem Solving (12)
E("Vocal Problem Solving | Plosive Guard Dynamic", [band(mode=DYN, typ=LS, freq=120, q=0.7, thr=-22, rng=10)], hp=60)
E("Vocal Problem Solving | Boxy Room Cut", [band(freq=450, gain=-4, q=2)], hp=60)
E("Vocal Problem Solving | Honky 1k Cut", [band(freq=1000, gain=-4, q=2.5)], hp=60)
E("Vocal Problem Solving | Harsh 3k Dynamic", [band(mode=DYN, freq=3000, q=2, thr=-30, rng=8)], hp=60)
E("Vocal Problem Solving | Whistle Notch", [band(typ=NOTCH, freq=6000, q=8)], hp=60)
E("Vocal Problem Solving | Hum Notch 60", [band(typ=NOTCH, freq=60, q=10), band(typ=NOTCH, freq=120, q=10), band(typ=NOTCH, freq=180, q=10)], hp=40)
E("Vocal Problem Solving | Hum Notch 50", [band(typ=NOTCH, freq=50, q=10), band(typ=NOTCH, freq=100, q=10), band(typ=NOTCH, freq=150, q=10)], hp=35)
E("Vocal Problem Solving | Resonance Follow", [band(mode=PT, harm=2, gain=-3, q=5), band(mode=PT, harm=3, gain=-2, q=5)], hp=70)
E("Vocal Problem Solving | Sibilant 7k Dynamic", [band(mode=DYN, freq=7000, q=2.5, thr=-34, rng=8)], hp=60)
E("Vocal Problem Solving | Low Rumble Remove", [], hp=100, hps=3)
E("Vocal Problem Solving | Muffled Mic Lift", [band(freq=400, gain=-2, q=1), band(freq=4000, gain=3, q=0.9), band(typ=HS, freq=10000, gain=3, q=0.7)], hp=80)
E("Vocal Problem Solving | Breath Lows Dynamic", [band(mode=DYN, freq=250, q=1, thr=-30, rng=6), band(mode=DYN, freq=500, q=1.2, thr=-32, rng=4)], hp=80)
# Air & Tone (8)
E("Vocal Air & Tone | Silk Air 16k", [], hp=60, air=4, airf=16000)
E("Vocal Air & Tone | Super Air 25k", [], hp=60, air=6, airf=25000)
E("Vocal Air & Tone | Gentle Air 12k", [], hp=60, air=2.5, airf=12000)
E("Vocal Air & Tone | Tilt Bright", [], hp=60, tilt=2.5)
E("Vocal Air & Tone | Tilt Warm", [], hp=60, tilt=-2.5)
E("Vocal Air & Tone | Presence Shelf", [band(typ=HS, freq=4000, gain=2, q=0.7)], hp=60)
E("Vocal Air & Tone | Body Shelf", [band(typ=LS, freq=250, gain=2, q=0.7)], hp=60)
E("Vocal Air & Tone | Harmonic Sparkle", [band(mode=PT, harm=8, gain=2, q=2), band(mode=PT, harm=12, gain=2, q=2)], hp=60, air=1.5)
# Creative (6) - level exempt
E("Creative | Telephone Voice", [band(freq=1500, gain=6, q=1)], hp=500, hps=3, lp=3500, lps=3)
E("Creative | Megaphone", [band(freq=1200, gain=8, q=1.5), band(freq=2500, gain=4, q=2)], hp=700, hps=3, lp=5000, lps=3)
E("Creative | AM Radio", [band(freq=1000, gain=4, q=0.8)], hp=300, hps=2, lp=4500, lps=3)
E("Creative | Walkie Talkie", [band(freq=1800, gain=8, q=1.2)], hp=900, hps=3, lp=3000, lps=3)
E("Creative | Under Water", [band(freq=400, gain=4, q=1)], hp=60, lp=800, lps=3)
E("Creative | Formant Wobble", [band(mode=PT, harm=2, gain=8, q=4), band(mode=PT, harm=4, gain=6, q=4)], hp=100)
# Instrument (10) - pink target
E("Instrument | Acoustic Guitar", [band(freq=200, gain=-2, q=1), band(typ=HS, freq=8000, gain=2, q=0.7)], hp=80)
E("Instrument | Piano Clarity", [band(freq=300, gain=-1.5, q=1), band(freq=3000, gain=1.5, q=1)], hp=40)
E("Instrument | Bass Note Follow", [band(mode=PT, harm=1, gain=2, q=2), band(freq=800, gain=1.5, q=1.2)], hp=30)
E("Instrument | Sax Smooth", [band(mode=DYN, freq=2500, q=1.6, thr=-28, rng=5), band(freq=400, gain=-1, q=1)], hp=80)
E("Instrument | Violin Rosin Tamer", [band(mode=DYN, freq=3200, q=2, thr=-30, rng=5)], hp=150)
E("Instrument | Trumpet Bite", [band(freq=1500, gain=2, q=1), band(mode=DYN, freq=4000, q=2, thr=-30, rng=4)], hp=150)
E("Instrument | Flute Air", [band(freq=500, gain=-1.5, q=1)], hp=200, air=3, airf=14000)
E("Instrument | Synth Lead Follow", [band(mode=PT, harm=2, gain=2, q=3), band(mode=PT, harm=3, gain=1.5, q=3)], hp=80)
E("Instrument | Electric Guitar Fizz Cut", [band(freq=250, gain=-1, q=1)], hp=90, lp=9000, lps=2)
E("Instrument | Strings Warmth", [band(typ=LS, freq=250, gain=1.5, q=0.7), band(freq=3000, gain=-1, q=1.2)], hp=60)

#=====================================================================================================================
# Hush De-Ess 2
DE = []
def D(name, split=4500, hthr=-30, sthr=-34, hr=8, sr_=10, sens=50, link=False):
    DE.append(dict(name=name, split=split, hthr=hthr, sthr=sthr, hr=hr, sr=sr_, sens=sens, link=link))
for nm, sp, h, s, hr, sr_, se in [("Female Vocal | Gentle", 6000, -28, -32, 5, 6, 45), ("Female Vocal | Medium", 6000, -32, -36, 7, 9, 50), ("Female Vocal | Strong", 5500, -36, -40, 9, 12, 60),
                                 ("Female Vocal | Bright Pop", 6500, -32, -36, 7, 10, 55), ("Female Vocal | Breathy", 7000, -34, -38, 6, 10, 55), ("Female Vocal | Belter", 5500, -30, -34, 8, 10, 50),
                                 ("Female Vocal | Soft S Only", 6000, -10, -36, 0, 9, 50), ("Female Vocal | Airy Keep", 8000, -30, -34, 5, 7, 40),
                                 ("Male Vocal | Gentle", 4500, -28, -32, 5, 6, 45), ("Male Vocal | Medium", 4500, -32, -36, 7, 9, 50), ("Male Vocal | Strong", 4000, -36, -40, 9, 12, 60),
                                 ("Male Vocal | Baritone", 4000, -30, -34, 6, 9, 50), ("Male Vocal | Tenor", 5000, -32, -36, 7, 9, 50), ("Male Vocal | Rock Lead", 4500, -30, -34, 8, 10, 55),
                                 ("Male Vocal | Crooner", 4500, -30, -34, 6, 8, 45), ("Male Vocal | Soft S Only", 4500, -10, -36, 0, 9, 50),
                                 ("Rap Vocal | Upfront", 5000, -32, -36, 8, 10, 55), ("Rap Vocal | Trap Hi-Hat Safe", 5500, -34, -38, 9, 11, 60), ("Rap Vocal | Fast Flow", 5000, -30, -34, 7, 9, 55),
                                 ("Rap Vocal | Hard T and K", 5000, -36, -10, 10, 0, 55), ("Rap Vocal | Ad-Libs", 5500, -34, -38, 9, 12, 60), ("Rap Vocal | Melodic", 5500, -32, -36, 7, 9, 50),
                                 ("R&B Vocal | Silky", 6000, -32, -36, 6, 9, 50), ("R&B Vocal | Falsetto", 7000, -32, -36, 6, 9, 50), ("R&B Vocal | Smooth Runs", 6000, -30, -34, 6, 8, 45),
                                 ("R&B Vocal | Modern Gloss", 6500, -34, -38, 8, 10, 55),
                                 ("Backing Vocal | Stack", 5500, -36, -40, 10, 12, 60), ("Backing Vocal | Choir", 6000, -36, -40, 9, 12, 60), ("Backing Vocal | Doubles", 5500, -34, -38, 10, 12, 60),
                                 ("Backing Vocal | Airy Pads", 7000, -34, -38, 8, 10, 55),
                                 ("Voice-Over | Broadcast", 5000, -32, -36, 7, 9, 50), ("Voice-Over | Podcast", 5000, -30, -34, 6, 8, 50), ("Voice-Over | Audiobook", 5500, -30, -34, 5, 7, 45),
                                 ("Voice-Over | Close Mic", 5000, -34, -38, 8, 10, 55), ("Voice-Over | Interview", 5000, -30, -34, 6, 8, 50), ("Voice-Over | Trailer", 4500, -32, -36, 7, 9, 50),
                                 ("Rock Vocal | Screamer", 4500, -30, -34, 9, 10, 55), ("Rock Vocal | Indie", 5500, -30, -34, 6, 8, 45), ("Rock Vocal | Punk", 5000, -28, -32, 8, 9, 50),
                                 ("Live Vocal | Stage Mic", 5000, -30, -34, 7, 9, 50), ("Live Vocal | Bright PA", 5500, -32, -36, 8, 10, 55), ("Live Vocal | Worship", 5500, -30, -34, 6, 8, 50),
                                 ("Gospel Vocal | Lead", 5500, -30, -34, 7, 9, 50), ("Gospel Vocal | Choir Bus", 6000, -34, -38, 9, 11, 55),
                                 ("Country Vocal | Lead", 5500, -30, -34, 6, 8, 45), ("Country Vocal | Harmony", 6000, -32, -36, 7, 9, 50),
                                 ("Vintage Vocal | Ribbon Mic", 4500, -28, -32, 5, 6, 40), ("Vintage Vocal | Tape Era", 5000, -30, -34, 6, 8, 45),
                                 ("Vocal Problem Solving | Lisp Guard", 5000, -28, -30, 4, 5, 35), ("Vocal Problem Solving | Whistling S", 7500, -34, -38, 10, 14, 65),
                                 ("Vocal Problem Solving | Harsh Plosive T", 4500, -36, -10, 12, 0, 55), ("Vocal Problem Solving | Sizzle Only", 9000, -36, -40, 8, 12, 55),
                                 ("Vocal Problem Solving | Bright Condenser", 6000, -36, -40, 9, 12, 60), ("Vocal Problem Solving | Cymbal Bleed", 7000, -34, -38, 6, 8, 70),
                                 ("Instrument | Acoustic Guitar Squeak", 3500, -30, -34, 6, 8, 60), ("Instrument | Hi-Hat Tamer", 7000, -30, -34, 6, 8, 70),
                                 ("Instrument | Brass Fizz", 5000, -30, -34, 5, 7, 65), ("Instrument | Violin Rosin", 4500, -32, -36, 5, 7, 65),
                                 ("Creative | Lisp Effect", 3000, -40, -44, 20, 24, 80), ("Creative | Kill All S", 3500, -44, -48, 24, 24, 90)]:
    D(nm, sp, h, s, hr, sr_, se)

#=====================================================================================================================
# Moth Tune 2. Values by parameter NAME (as the app reads them); props by state id.
MT = []
def T(name, retune=20, human=0, flex=0, keep=100, natvib=0, strength=100, transpose=0, formant=0, throat=100, char=0, engine=1, preserve=1,
      dv=0, dvthr=15, dvdel=0.1, dvatk=0.15, dvvar=0, dvp=100, dva=0, dvt=0, vd=0, vr=5.5, vdl=0.3, vo=0.4, vv=20, vshape=0, straight=0,
      harm=None, hdet=6, htime=12, link=0, trig=0, latch=0, voice=1):
    vals = {"Retune Speed": retune, "Humanize": human, "Flex Tune": flex, "Vibrato Keep": keep, "Natural Vibrato": natvib, "Correction": strength,
            "Transpose": transpose, "Formant": formant, "Throat": throat, "Vibrato Threshold": dvthr, "Vibrato Detect Delay": dvdel, "Vibrato Attack": dvatk,
            "Vibrato Irregularity": dvvar, "Pitch Depth": dvp, "Amplitude Depth": dva, "Timbre Depth": dvt, "Vibrato Depth": vd, "Vibrato Rate": vr,
            "Vibrato Delay": vdl, "Vibrato Onset": vo, "Vibrato Variation": vv, "Harmony Detune": hdet, "Harmony Timing": htime}
    props = {"character": char, "engine": engine, "formantPreserve": preserve, "dvMode": dv, "vibShape": vshape, "vibFxStraight": straight,
             "formantLink": link, "harmTrigger": trig, "harmLatch": latch, "voiceType": voice}
    trim = 0.0
    if harm:
        tot = 1.0
        for i, h in enumerate(harm):   # (mode, interval, level dB, pan)
            m, iv, lv, pan = h
            props["harmMode%d" % (i + 1)] = m
            vals["Voice %d Interval" % (i + 1)] = iv; vals["Voice %d Level" % (i + 1)] = lv; vals["Voice %d Pan" % (i + 1)] = pan
            tot += 10 ** (lv / 10.0)
        trim = -10 * math.log10(tot)     # the harmony voices add their power: hold the overall level
    vals["Output"] = round(trim, 1)
    MT.append(dict(name=name, vals=vals, props=props))
P, CUT, BOOST = 1, 2, 3
# Natural (16)
T("Natural | Transparent Lead", 40, 30, 25, dv=P)
T("Natural | Invisible Fix", 60, 45, 40, dv=P, strength=80)
T("Natural | Gentle Pitch Assist", 80, 50, 50, dv=P, strength=70)
T("Natural | Studio Natural", 30, 25, 20, dv=P)
T("Natural | Keep The Vibrato", 15, 20, 15, dv=P, dvthr=10)
T("Natural | Long Notes Only", 50, 70, 30, dv=P)
T("Natural | Flex Heavy", 35, 30, 70, dv=P)
T("Natural | Soft Correction 60%", 30, 25, 20, dv=P, strength=60)
T("Natural | Ballad Breath", 70, 55, 45, dv=P, dvdel=0.2)
T("Natural | Acoustic Singer", 45, 40, 35, dv=P)
T("Natural | Folk Purist", 120, 60, 60, dv=P, strength=60)
T("Natural | Classical Legato", 90, 60, 50, dv=P, dvthr=8, voice=0)
T("Natural | Jazz Phrasing", 100, 60, 60, dv=P, strength=65)
T("Natural | Low Voice Natural", 40, 35, 25, dv=P, voice=2)
T("Natural | Soprano Natural", 40, 35, 25, dv=P, voice=0)
T("Natural | Hardly There", 150, 70, 80, dv=P, strength=50)
# Pop Polish (16)
T("Pop Polish | Radio Pop Lead", 15, 20, 10, dv=P)
T("Pop Polish | Glossy Chorus", 10, 15, 5, dv=P)
T("Pop Polish | Tight Verse", 12, 10, 5, dv=CUT, dvp=40)
T("Pop Polish | Polished Not Robotic", 18, 30, 15, dv=P)
T("Pop Polish | Modern Pop Snap", 8, 10, 0, dv=CUT, dvp=60)
T("Pop Polish | Teen Pop Shine", 10, 10, 5, dv=P, formant=0.3)
T("Pop Polish | Dance Pop Lead", 6, 5, 0, dv=CUT, dvp=70)
T("Pop Polish | Straight Notes Pop", 10, 10, 5, dv=CUT, dvp=100, dva=50)
T("Pop Polish | Wide Pop Doubles", 15, 20, 10, dv=P, harm=[(1, 0, -5, -80), (1, 0, -5, 80)], hdet=10, htime=20)
T("Pop Polish | Pop Harmony Thirds", 15, 15, 5, dv=P, harm=[(2, 2, -7, -40)])
T("Pop Polish | Pop Stack 3rd and 5th", 15, 15, 5, dv=P, harm=[(2, 2, -8, -45), (2, 4, -9, 45)])
T("Pop Polish | Octave Lift Chorus", 12, 10, 5, dv=P, harm=[(1, 12, -12, 0)])
T("Pop Polish | Polished Belter", 20, 25, 15, dv=P, dva=20)
T("Pop Polish | Clean Hook", 8, 10, 0, dv=CUT, dvp=50)
T("Pop Polish | Bedroom Pop", 25, 30, 20, dv=P, formant=-0.3)
T("Pop Polish | Shimmer Boost Vibrato", 15, 15, 10, dv=BOOST, dvp=40, dva=20)
# R&B (14)
T("R&B | Smooth Runs", 10, 20, 5, dv=P)
T("R&B | Neo Soul Natural", 30, 35, 25, dv=P)
T("R&B | Modern R&B Lead", 8, 15, 5, dv=P)
T("R&B | Falsetto Control", 15, 20, 10, dv=P, voice=0)
T("R&B | Silky Vibrato Boost", 15, 20, 10, dv=BOOST, dvp=30, dva=15, dvt=10)
T("R&B | Late Night", 20, 30, 15, dv=P, formant=-0.4)
T("R&B | Melisma Friendly", 12, 25, 30, dv=P)
T("R&B | Tight Ad-Libs", 5, 5, 0, dv=CUT, dvp=60)
T("R&B | Harmony Stack R&B", 12, 20, 5, dv=P, harm=[(2, 2, -8, -50), (2, 4, -9, 50), (2, -3, -11, 0)])
T("R&B | Warm Throat", 15, 20, 10, dv=P, throat=108)
T("R&B | Slow Jam", 25, 35, 20, dv=P)
T("R&B | Vibrato Cut Runs", 10, 15, 5, dv=CUT, dvp=70, dvdel=0.05)
T("R&B | Gospel-Tinged R&B", 18, 30, 15, dv=P, dva=10)
T("R&B | Crisp Modern", 6, 10, 0, dv=P)
# Hip-Hop / Trap Hard (16)
T("Hip-Hop / Trap Hard | Hard Tune Lead", 0, 0, 0, keep=0, char=1)
T("Hip-Hop / Trap Hard | Trap Lead Classic", 0, 0, 0, keep=0, char=1, formant=0.4)
T("Hip-Hop / Trap Hard | Melodic Rap Snap", 2, 5, 0, dv=CUT, dvp=100, char=1)
T("Hip-Hop / Trap Hard | Robotic Steps", 0, 0, 0, keep=0, natvib=-100, char=1)
T("Hip-Hop / Trap Hard | Hard Tune Keep Vibrato", 0, 0, 0, dv=P, char=1)
T("Hip-Hop / Trap Hard | Drill Hook", 0, 0, 0, keep=0, char=1, formant=-0.5)
T("Hip-Hop / Trap Hard | Auto Croon", 5, 5, 0, dv=CUT, dvp=80, char=0)
T("Hip-Hop / Trap Hard | Ad-Lib Robot", 0, 0, 0, keep=0, char=1, harm=[(1, 12, -14, 0)])
T("Hip-Hop / Trap Hard | Octave Down Ad-Libs", 0, 0, 0, keep=0, char=1, harm=[(1, -12, -10, 0)])
T("Hip-Hop / Trap Hard | Hard Tune Doubles", 0, 0, 0, keep=0, char=1, harm=[(1, 0, -6, -90), (1, 0, -6, 90)], hdet=8, htime=18)
T("Hip-Hop / Trap Hard | Glitchy Steps", 0, 0, 0, keep=0, char=1, natvib=-100, strength=100, throat=96)
T("Hip-Hop / Trap Hard | Smooth Hard Tune", 4, 0, 0, keep=0, char=0)
T("Hip-Hop / Trap Hard | Wobble Boost", 2, 0, 0, dv=BOOST, dvp=120, dvt=30, char=1)
T("Hip-Hop / Trap Hard | Emo Rap Lead", 3, 5, 0, dv=CUT, dvp=60, char=1)
T("Hip-Hop / Trap Hard | Live Trap (low latency)", 0, 0, 0, keep=0, char=1, engine=0)
T("Hip-Hop / Trap Hard | Rage Lead", 0, 0, 0, keep=0, char=1, formant=0.8, throat=92)
# Country (10)
T("Country | Nashville Natural", 30, 30, 25, dv=P)
T("Country | Modern Country Pop", 15, 20, 10, dv=P)
T("Country | Twang Keep", 40, 35, 35, dv=P, dvthr=10)
T("Country | Bluegrass Purist", 80, 50, 50, dv=P, strength=70)
T("Country | Country Harmony Fifth", 25, 25, 15, dv=P, harm=[(2, 4, -8, 40)])
T("Country | Duet Third Above", 25, 25, 15, dv=P, harm=[(2, 2, -7, -40)])
T("Country | Ballad Warmth", 45, 40, 35, dv=P, throat=104)
T("Country | Honky-Tonk Lead", 25, 25, 20, dv=P)
T("Country | Vibrato Lift", 30, 30, 20, dv=BOOST, dvp=30)
T("Country | Straight Note Country", 20, 20, 15, dv=CUT, dvp=50)
# Rock (10)
T("Rock | Rock Lead Natural", 35, 30, 30, dv=P)
T("Rock | Arena Rock Tight", 18, 20, 15, dv=P)
T("Rock | Indie Imperfect", 70, 50, 60, dv=P, strength=60)
T("Rock | Grunge Loose", 100, 60, 70, dv=P, strength=50)
T("Rock | Pop Punk Tight", 10, 10, 5, dv=CUT, dvp=50)
T("Rock | Metal Clean Chorus", 15, 15, 10, dv=P)
T("Rock | Screamer Safe", 50, 40, 60, dv=P, strength=60)
T("Rock | Rock Doubles", 25, 25, 20, dv=P, harm=[(1, 0, -5, -85), (1, 0, -5, 85)], hdet=12, htime=24)
T("Rock | Power Fifth Chorus", 20, 20, 15, dv=P, harm=[(1, 7, -9, 30)])
T("Rock | Ballad Rock", 30, 35, 25, dv=P, dva=10)
# Gospel (10)
T("Gospel | Praise Lead", 15, 25, 15, dv=P)
T("Gospel | Choir Stack", 20, 25, 15, dv=P, harm=[(2, 2, -8, -60), (2, 4, -9, 60), (2, -3, -10, -30), (2, 7, -12, 30)], hdet=14, htime=24)
T("Gospel | Run Friendly", 10, 25, 30, dv=P)
T("Gospel | Big Vibrato", 20, 25, 15, dv=BOOST, dvp=35, dva=25, dvt=15)
T("Gospel | Soul Natural", 35, 35, 30, dv=P)
T("Gospel | Choir Thirds and Sixths", 20, 25, 15, dv=P, harm=[(2, 2, -8, -45), (2, 5, -10, 45)])
T("Gospel | Worship Pad Harmonies", 25, 30, 20, dv=P, harm=[(2, 2, -10, -50), (2, 4, -11, 50)], hdet=16, htime=30)
T("Gospel | Belting Lead", 18, 25, 15, dv=P, dva=15)
T("Gospel | Sunday Morning", 30, 35, 25, dv=P)
T("Gospel | Quartet Octave", 20, 25, 15, dv=P, harm=[(1, -12, -9, 0)])
# Live Stage (12): Live engine, under 1 ms
T("Live Stage | Live Natural", 30, 30, 20, dv=P, engine=0)
T("Live Stage | Live Pop", 15, 20, 10, dv=P, engine=0)
T("Live Stage | Live Hard Tune", 0, 0, 0, keep=0, char=1, engine=0)
T("Live Stage | Live R&B", 10, 20, 5, dv=P, engine=0)
T("Live Stage | Live Rock", 35, 30, 30, dv=P, engine=0)
T("Live Stage | Live Worship", 25, 30, 20, dv=P, engine=0)
T("Live Stage | Live Doubles", 20, 25, 15, dv=P, engine=0, harm=[(1, 0, -6, -80), (1, 0, -6, 80)], hdet=10, htime=20)
T("Live Stage | Live Harmony on Notes", 15, 20, 10, dv=P, engine=0, harm=[(2, 2, -8, -45), (2, 4, -9, 45)], trig=1)
T("Live Stage | Live Harmony on Pedal CC64", 15, 20, 10, dv=P, engine=0, harm=[(2, 2, -8, -45), (2, 4, -9, 45)], trig=2)
T("Live Stage | Live Latch Harmonies", 15, 20, 10, dv=P, engine=0, harm=[(2, 2, -8, -45), (2, 4, -9, 45)], trig=2, latch=1)
T("Live Stage | Live Rap Snap", 3, 5, 0, dv=CUT, dvp=80, char=1, engine=0)
T("Live Stage | Live Gentle", 50, 40, 40, dv=P, strength=70, engine=0)
# Instrument (10)
T("Instrument | Fretless Bass", 30, 20, 20, dv=P, voice=4)
T("Instrument | Violin Intonation", 50, 40, 40, dv=P, voice=3)
T("Instrument | Trumpet Tight", 25, 20, 15, dv=P, voice=3)
T("Instrument | Sax Natural", 40, 35, 30, dv=P, voice=3)
T("Instrument | Cello Warm", 45, 40, 35, dv=P, voice=4)
T("Instrument | Whistle Fix", 20, 20, 10, dv=P, voice=3)
T("Instrument | Synth Lead Hard", 0, 0, 0, keep=0, char=1, voice=3)
T("Instrument | Slide Guitar Snap", 10, 5, 0, dv=CUT, dvp=60, voice=3)
T("Instrument | Theremin Tamer", 20, 10, 10, dv=CUT, dvp=40, voice=3)
T("Instrument | Bass Hard Tune", 0, 0, 0, keep=0, char=1, voice=4)
# Vintage (10)
T("Vintage | Late 90s Hard Tune", 0, 0, 0, keep=0, char=1, natvib=-100)
T("Vintage | Classic Retune 10", 10, 0, 0, keep=0, char=1)
T("Vintage | Early 2000s R&B", 5, 5, 0, keep=30, char=1)
T("Vintage | Classic Chipmunk Soul", 15, 10, 5, dv=P, transpose=5, preserve=0, formant=0)
T("Vintage | Old Pitch Box", 20, 0, 0, keep=50, char=1)
T("Vintage | 80s Harmonizer", 20, 15, 10, dv=P, harm=[(1, 7, -10, 30), (1, -5, -12, -30)])
T("Vintage | Crooner Wide Vibrato", 30, 30, 20, dv=BOOST, dvp=50, dva=20)
T("Vintage | Doo-Wop Blend", 25, 25, 20, dv=P, harm=[(2, 2, -9, -40), (2, 4, -10, 40)])
T("Vintage | Tape Warble Vibrato", 20, 20, 10, dv=CUT, dvp=100, vd=18, vr=4.2, vdl=0.1, vo=0.2, vv=40, straight=1)
T("Vintage | Slow Classic Retune", 40, 10, 0, keep=0, char=1)
# Creative (16) - level exempt
T("Creative | Robot Monotone", 0, 0, 0, keep=0, natvib=-100, char=1, strength=100)
T("Creative | Chipmunk", 10, 0, 0, transpose=12, preserve=0)
T("Creative | Giant Voice", 15, 0, 0, transpose=-12, formant=-3, throat=125)
T("Creative | Gender Flip Down", 25, 0, 0, transpose=-12, formant=-2.5, throat=114, link=0)
T("Creative | Gender Flip Up", 25, 0, 0, transpose=12, formant=2.5, throat=88)
T("Creative | Linked Formant Shift Up", 20, 0, 0, transpose=5, link=1)
T("Creative | Linked Formant Shift Down", 20, 0, 0, transpose=-5, link=1)
T("Creative | Opera Wobble", 20, 20, 10, dv=BOOST, dvp=150, dva=60, dvt=40, dvvar=50)
T("Creative | Square Vibrato FX", 10, 0, 0, dv=CUT, vd=60, vr=6.5, vdl=0.05, vo=0.05, vv=0, vshape=2, straight=1)
T("Creative | Saw Vibrato FX", 10, 0, 0, dv=CUT, vd=50, vr=4, vdl=0.05, vo=0.1, vv=0, vshape=3, straight=1)
T("Creative | Triangle Siren", 10, 0, 0, keep=0, vd=120, vr=1.5, vdl=0, vo=0, vv=0, vshape=1)
T("Creative | Choir of Robots", 0, 0, 0, keep=0, char=1, harm=[(2, 2, -6, -70), (2, 4, -7, 70), (2, -3, -8, -35), (1, 12, -10, 35)])
T("Creative | Fifths Machine", 0, 0, 0, keep=0, char=1, harm=[(1, 7, -6, -50), (1, -5, -7, 50)])
T("Creative | Octave Stack", 5, 0, 0, keep=0, harm=[(1, 12, -8, -30), (1, -12, -8, 30)])
T("Creative | Detuned Chorus Voice", 20, 10, 5, dv=P, harm=[(1, 0, -4, -100), (1, 0, -4, 100)], hdet=25, htime=35)
T("Creative | Talkbox Glide", 45, 0, 0, keep=0, char=0, formant=1.5)
T("Pop Polish | Synth-Pop Sheen", 6, 5, 0, dv=CUT, dvp=80, formant=0.2)
T("Pop Polish | Acoustic Pop", 30, 30, 25, dv=P)
T("R&B | Quiet Storm", 35, 40, 25, dv=P, dva=10)
T("R&B | Harmony Sixths", 15, 20, 10, dv=P, harm=[(2, 5, -9, 40)])
T("Natural | Spoken Word Pitch", 120, 70, 80, dv=P, strength=40)
T("Natural | Choir Section", 50, 45, 40, dv=P, voice=0)
T("Rock | Stadium Harmony", 20, 20, 15, dv=P, harm=[(2, 2, -9, -50), (2, 4, -10, 50)])
T("Country | Close Harmony Trio", 25, 25, 15, dv=P, harm=[(2, 2, -8, -45), (2, -3, -9, 45)])
T("Gospel | Vamp Lead", 12, 25, 25, dv=P, dvthr=12)
T("Instrument | Pedal Steel", 25, 15, 20, dv=P, voice=3)

#=====================================================================================================================
def write_table(path):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111v.py - do not edit.\n#pragma once\n')
        f.write('struct CompRow { const char* name; int m1, fet1; float thr1, atk1, rel1, ratio1, knee; int dual, m2, fet2; float thr2, atk2, rel2, ratio2, warm, mix; int punch; float pgain, impact, ceil, duck; };\n')
        f.write('static const CompRow compPresets[] = {\n')
        for p in COMP:
            f.write('    { %s, %d, %d, %g, %g, %g, %g, %g, %d, %d, %d, %g, %g, %g, %g, %g, %g, %d, %g, %g, %g, %g },\n' % (json.dumps(p['name']), p['m1'], p['fet1'], p['thr1'], p['atk1'], p['rel1'], p['ratio1'], p['knee'],
                    int(p['dual']), p['m2'], p['fet2'], p['thr2'], p['atk2'], p['rel2'], p['ratio2'], p['warm'], p['mix'], int(p['punch']), p['pgain'], p['impact'], p['ceil'], p['duck']))
        f.write('};\n')
        f.write('struct EqBandRow { int on, type, mode, harm; float freq, gain, q, thr, range; int expand; };\n')
        f.write('struct EqRow { const char* name; EqBandRow b[6]; float hp; int hps; float lp; int lps; float airf, air, tilt; };\n')
        f.write('static const EqRow eqPresets[] = {\n')
        for p in EQ:
            bs = ', '.join('{ %d, %d, %d, %d, %g, %g, %g, %g, %g, %d }' % (int(b['on']), b['type'], b['mode'], b['harm'], b['freq'], b['gain'], b['q'], b['thr'], b['range'], int(b['expand'])) for b in p['bands'])
            f.write('    { %s, { %s }, %g, %d, %g, %d, %g, %g, %g },\n' % (json.dumps(p['name']), bs, p['hp'], p['hps'], p['lp'], p['lps'], p['airf'], p['air'], p['tilt']))
        f.write('};\n')
        f.write('struct DeRow { const char* name; float split, hthr, sthr, hr, sr, sens; int link; };\n')
        f.write('static const DeRow dePresets[] = {\n')
        for p in DE:
            f.write('    { %s, %g, %g, %g, %g, %g, %g, %d },\n' % (json.dumps(p['name']), p['split'], p['hthr'], p['sthr'], p['hr'], p['sr'], p['sens'], int(p['link'])))
        f.write('};\n')

def calibrate():
    exe = '/tmp/preset_calib_voice'
    src = os.path.join(ROOT, 'tests', 'preset_calib_voice.cpp')
    subprocess.check_call(['g++', '-O2', '-std=c++17', '-I' + os.path.join(ROOT, 'Source'), '-I' + os.path.join(ROOT, 'tests'), src, '-o', exe])
    out = subprocess.check_output([exe], text=True)
    trims = {}
    for line in out.splitlines():
        if line.startswith('TRIM\t'):
            _, plug, name, trim, spread = line.split('\t')
            trims[(plug, name)] = (float(trim), float(spread))
    return trims

def num(v):
    s = ('%.2f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def emit(trims):
    L = []
    L.append('        // Moth Tune 2 props: engine (0 Live, 1 Studio), character (0 Modern, 1 Classic), dvMode (0 off, 1 Preserve, 2 Cut, 3 Boost), voiceType,')
    L.append('        // formantPreserve, formantLink, vibShape, vibFxStraight, harmTrigger (0 always, 1 MIDI note, 2 CC), harmLatch, harmMode1-4.')
    L.append('        { "Moth Tune", {')
    for p in MT:
        vals = ', '.join('N (%s, %s)' % (json.dumps(k), num(v)) for k, v in p['vals'].items())
        props = ', '.join('{ "%s", %d }' % (k, v) for k, v in p['props'].items())
        L.append('            B (%s, { %s },\n                { %s }),' % (json.dumps(p['name']), vals, props))
    L.append('        } },')
    L.append('        // Siren Comp props: model1 / model2 (0 FET, 1 Opto A, 2 Opto B, 3 Modern), fet1 / fet2 (0 4:1 .. 4 All), dualOn, autoMakeupOn, punchOn. Output = measured trim.')
    L.append('        { "Siren Comp", {')
    for p in COMP:
        t = max(-24.0, min(12.0, trims[('Siren Comp', p['name'])][0]))
        vals = ['N ("thr1", %s)' % num(p['thr1']), 'N ("atk1", %s)' % num(p['atk1']), 'N ("rel1", %s)' % num(p['rel1']), 'N ("ratio1", %s)' % num(p['ratio1']), 'N ("knee1", %s)' % num(p['knee']),
                'N ("thr2", %s)' % num(p['thr2']), 'N ("atk2", %s)' % num(p['atk2']), 'N ("rel2", %s)' % num(p['rel2']), 'N ("ratio2", %s)' % num(p['ratio2']),
                'N ("warm", %s)' % num(p['warm']), 'N ("mix", %s)' % num(p['mix']), 'N ("makeup", 0)', 'N ("output", %s)' % num(t),
                'N ("pGain", %s)' % num(p['pgain']), 'N ("pImpact", %s)' % num(p['impact']), 'N ("pCeil", %s)' % num(p['ceil']), 'N ("duck", %s)' % num(p['duck'])]
        props = ['{ "model1", %d }' % p['m1'], '{ "fet1", %d }' % p['fet1'], '{ "model2", %d }' % p['m2'], '{ "fet2", %d }' % p['fet2'], '{ "dualOn", %d }' % int(p['dual']),
                 '{ "autoMakeupOn", 0 }', '{ "punchOn", %d }' % int(p['punch'])]
        L.append('            B (%s, { %s },\n                { %s }),' % (json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    L.append('        } },')
    L.append('        // Siren EQ props per band N: bNOn, bNType (0 bell 1 low shelf 2 high shelf 3 notch), bNMode (0 fixed 1 pitch 2 dynamic), bNHarm, bNExpand; hpSlope / lpSlope (0-3).')
    L.append('        { "Siren EQ", {')
    for p in EQ:
        t = max(-24.0, min(12.0, trims[('Siren EQ', p['name'])][0]))
        vals, props = [], []
        for i, b in enumerate(p['bands']):
            n = i + 1
            if not b['on']: continue
            vals += ['N ("b%dFreq", %s)' % (n, num(b['freq'])), 'N ("b%dGain", %s)' % (n, num(b['gain'])), 'N ("b%dQ", %s)' % (n, num(b['q'])),
                     'N ("b%dThr", %s)' % (n, num(b['thr'])), 'N ("b%dRange", %s)' % (n, num(b['range']))]
            props += ['{ "b%dOn", 1 }' % n, '{ "b%dType", %d }' % (n, b['type']), '{ "b%dMode", %d }' % (n, b['mode']), '{ "b%dHarm", %d }' % (n, b['harm']), '{ "b%dExpand", %d }' % (n, int(b['expand']))]
        vals += ['N ("hpFreq", %s)' % num(p['hp']), 'N ("lpFreq", %s)' % num(p['lp']), 'N ("airFreq", %s)' % num(p['airf']), 'N ("airGain", %s)' % num(p['air']),
                 'N ("tilt", %s)' % num(p['tilt']), 'N ("output", %s)' % num(t)]
        props += ['{ "hpSlope", %d }' % p['hps'], '{ "lpSlope", %d }' % p['lps']]
        L.append('            B (%s, { %s },\n                { %s }),' % (json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    L.append('        } },')
    L.append('        // Hush De-Ess 2 props: linkOn. Output = measured trim.')
    L.append('        { "Hush De-Ess 2", {')
    for p in DE:
        t = max(-24.0, min(12.0, trims[('Hush De-Ess 2', p['name'])][0]))
        L.append('            B (%s, { N ("split", %s), N ("hardThr", %s), N ("softThr", %s), N ("hardRange", %s), N ("softRange", %s), N ("sens", %s), N ("output", %s) }, { { "linkOn", %d } }),' % (
            json.dumps(p['name']), num(p['split']), num(p['hthr']), num(p['sthr']), num(p['hr']), num(p['sr']), num(p['sens']), num(t), int(p['link'])))
    L.append('        } },')
    return '\n'.join(L)

def main():
    for lst, nm, want in ((MT, 'Moth Tune', 150), (COMP, 'Siren Comp', 110), (EQ, 'Siren EQ', 100), (DE, 'Hush De-Ess 2', 60)):
        names = [p['name'] for p in lst]
        assert len(names) == len(set(names)), 'duplicate preset names in ' + nm
        cats = sorted(set(n.split(' | ')[0] for n in names))
        print('%s: %d presets (want %d) in %d categories: %s' % (nm, len(lst), want, len(cats), ', '.join(cats)))
        assert len(lst) >= want, nm + ' is short of presets'
    write_table(os.path.join(ROOT, 'tests', 'presets111v_table.h'))
    trims = calibrate()
    blocks = emit(trims)
    fns, names, cur, comments = [], [], None, []
    for line in blocks.split('\n'):
        st = line.strip()
        if cur is None and st.startswith('//'): comments.append(line); continue
        m = re.match(r'\s*\{ "([^"]+)", \{$', line)
        if m:
            cur = m.group(1); names.append(cur); idx = len(names)
            fns.append('\n'.join(comments)); comments = []
            fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111v_%d()   // %s\n{\n    return {' % (idx, cur))
            continue
        if st == '} },' and cur is not None:
            fns.append('    };\n}\n'); cur = None; continue
        if cur is not None: fns.append(line.replace('            ', '        ', 1))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111v.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111v.py - factory presets for the 1.11 Voice family (first wave). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111.inl; bank() calls add111v().\n\n')
        f.write('\n'.join(x for x in fns if x) + '\n')
        f.write('inline void add111v (std::map<juce::String, std::vector<Preset>>& m)\n{\n')
        for k, nm in enumerate(names):
            if nm == 'Moth Tune':   # Moth Tune 2's 150 come first; the 1.6 presets stay (sessions and scripts use their names)
                f.write('    { auto v = presets111v_%d(); for (auto& p : m["Moth Tune"]) v.push_back (p); m["Moth Tune"] = std::move (v); }\n' % (k + 1))
            else: f.write('    m[%s] = presets111v_%d();\n' % (json.dumps(nm), k + 1))
        f.write('}\n')
    worst = max(trims.values(), key=lambda t: abs(t[0]))
    print('wrote', out, '- largest trim %.2f dB' % worst[0])

if __name__ == '__main__':
    main()
