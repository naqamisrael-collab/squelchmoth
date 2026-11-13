#!/usr/bin/env python3
"""Generates Source/SignaturePresets114.inl: factory presets for the 1.14 Studio Line (Spectra EQ first).
Every preset is an original Squelchmoth setting. A band: (shape, freq, gain, q, slope, place, dyn) where
dyn = None or (threshold, range, attack, release)."""
import os

SHAPES = {"bell": 0, "notch": 1, "lowshelf": 2, "highshelf": 3, "lowcut": 4, "highcut": 5, "bandpass": 6, "tilt": 7, "flattilt": 8, "allpass": 9}
PLACES = {"st": 0, "l": 1, "r": 2, "m": 3, "s": 4}

def band(shape, freq, gain=0.0, q=None, slope=12, place="st", dyn=None):
    if q is None: q = 0.707 if shape in ("lowcut", "highcut") else 1.0
    return (shape, freq, gain, q, slope, place, dyn)

# (name, advanced, phase, bands, output, autoGain)
P = []
def add(name, bands, adv=False, phase=0, out=0.0, auto=False): P.append((name, adv, phase, bands, out, auto))

# ---- Basic (the first preset is what a new Spectra EQ starts with: flat, no bands)
add("Flat | Start Here", [])
add("Mix | Clean Start", [band("lowcut", 30, slope=24)])
add("Vocal | Clarity", [band("lowcut", 90, slope=18), band("bell", 280, -2.5, 1.4), band("bell", 3200, 2.0, 1.1), band("highshelf", 11000, 2.5, 0.7)])
add("Vocal | Rap Upfront", [band("lowcut", 110, slope=24), band("bell", 220, -3.0, 1.6), band("bell", 1800, 2.5, 1.2), band("bell", 5200, 1.5, 2.0), band("highshelf", 12000, 2.0, 0.7)])
add("Vocal | Tame Harsh Esses", [band("lowcut", 80, slope=18), band("bell", 6800, 0.0, 3.0, dyn=(-32, -6, 1.0, 60))])
add("Vocal | Dynamic Mud Control", [band("lowcut", 80, slope=18), band("bell", 300, 0.0, 1.2, dyn=(-28, -5, 8, 140)), band("highshelf", 10000, 1.5, 0.7)])
add("Vocal | Air Lift", [band("highshelf", 9000, 3.0, 0.6), band("bell", 15000, 1.5, 0.8)])
add("Drums | Kick Punch", [band("lowcut", 28, slope=24), band("bell", 58, 3.0, 1.6), band("bell", 320, -4.0, 1.8), band("bell", 3600, 3.0, 1.5)])
add("Drums | Snare Crack", [band("lowcut", 90, slope=18), band("bell", 210, 2.0, 1.4), band("bell", 900, -2.5, 2.0), band("bell", 5200, 3.0, 1.2)])
add("Drums | Hi-Hat Smooth", [band("lowcut", 400, slope=24), band("bell", 8200, 0.0, 2.5, dyn=(-30, -5, 2, 80)), band("highshelf", 14000, 1.5, 0.7)])
add("Drums | Bus Weight", [band("lowshelf", 90, 2.0, 0.7), band("bell", 450, -1.5, 1.0), band("highshelf", 9000, 1.5, 0.7)])
add("808 | Sub Focus", [band("lowcut", 25, slope=36), band("bell", 50, 2.0, 1.2), band("bell", 180, -3.0, 1.4), band("highcut", 5000, slope=24)])
add("Bass | Definition", [band("lowcut", 32, slope=24), band("bell", 90, 1.5, 1.0), band("bell", 260, -2.5, 1.6), band("bell", 900, 2.0, 1.4)])
add("Guitar | Acoustic Sparkle", [band("lowcut", 90, slope=18), band("bell", 220, -2.5, 1.2), band("bell", 2600, 1.5, 1.0), band("highshelf", 10000, 2.5, 0.7)])
add("Keys | Piano Brightness", [band("lowcut", 40, slope=12), band("bell", 300, -1.5, 1.0), band("highshelf", 7000, 2.0, 0.7)])
add("Pads | Make Room", [band("lowcut", 160, slope=24), band("bell", 2500, -2.5, 0.8), band("highcut", 13000, slope=12)])
add("Master | Gentle Polish", [band("lowcut", 22, slope=24), band("lowshelf", 70, 0.8, 0.6), band("bell", 300, -0.8, 0.9), band("highshelf", 12000, 1.0, 0.6)], phase=1)
add("Master | Loud and Clear", [band("lowcut", 25, slope=36), band("bell", 55, 1.0, 1.0), band("bell", 250, -1.2, 1.0), band("bell", 3000, 0.8, 0.7), band("highshelf", 11000, 1.5, 0.6)], phase=1)
add("Master | Mono Low End", [band("lowcut", 22, slope=24), band("lowcut", 120, slope=12, place="s")], phase=2)
add("Master | Tilt Brighter", [band("flattilt", 1000, 0.5)], phase=1)
add("Master | Tilt Warmer", [band("flattilt", 1000, -0.5)], phase=1)
add("Mix | Side Air", [band("highshelf", 8000, 2.5, 0.7, place="s"), band("lowcut", 150, slope=12, place="s")])
add("Mix | Telephone", [band("lowcut", 400, slope=36), band("highcut", 3200, slope=36), band("bell", 1500, 4.0, 1.4)])
add("Mix | Radio Lo-Fi", [band("bandpass", 1200, q=0.6, slope=24), band("bell", 2500, 3.0, 1.0)])
add("Mix | Dark and Warm", [band("highcut", 9000, slope=12), band("lowshelf", 150, 2.0, 0.7)])

# ---- Advanced
add("Surgical | Ring Notch 1k", [band("notch", 1000, q=12, slope=12)], adv=True)
add("Surgical | Hum 60 Hz + Harmonics", [band("notch", 60, q=30), band("notch", 120, q=30), band("notch", 180, q=30), band("notch", 240, q=30)], adv=True)
add("Surgical | Hum 50 Hz + Harmonics", [band("notch", 50, q=30), band("notch", 100, q=30), band("notch", 150, q=30), band("notch", 200, q=30)], adv=True)
add("Surgical | Brickwall 20 Hz - 20 kHz", [band("lowcut", 20, slope=0), band("highcut", 20000, slope=0)], adv=True, phase=2)
add("Dynamic | Vocal Rider (Upward)", [band("bell", 2500, 0.0, 0.5, dyn=(-36, 4, 20, 250))], adv=True)
add("Dynamic | Kick Ducks Bass", [band("bell", 60, 0.0, 1.0, dyn=(-24, -6, 1, 90))], adv=True)
add("Dynamic | De-Box Guitar", [band("bell", 450, 0.0, 1.8, dyn=(-30, -6, 6, 120))], adv=True)
add("Creative | Sweepable Band Pass", [band("bandpass", 800, q=2.0, slope=48)], adv=True)
add("Creative | Phase Twist (All Pass)", [band("allpass", 300, q=0.7), band("allpass", 2000, q=0.7)], adv=True)
add("Mastering | Linear Phase Glue", [band("lowcut", 20, slope=48), band("lowshelf", 100, 0.5, 0.5), band("highshelf", 10000, 0.8, 0.5)], adv=True, phase=2, auto=True)
add("Mastering | Mid Focus, Wide Sides", [band("bell", 2000, 1.0, 0.7, place="m"), band("highshelf", 6000, 1.5, 0.7, place="s"), band("lowcut", 100, slope=12, place="s")], adv=True, phase=1)


# ---------------------------------------------------------------- Torque Comp
STY = {n: i for i, n in enumerate(["Clean","Punch","Glue","Opto","Variable Mu","FET","Bus","Vocal","Mastering","Leveler","Upward","Pump","Smash","Transparent Peak"])}
T = []
def tadd(name, style, adv=False, key=None, props=None, **v):
    # v: threshold ratio knee range attack release hold lookahead link drive mix makeup output; props: raw state
    T.append((name, adv, style, v, props or {}, key or []))

tadd("Start | Gentle 2:1", "Clean", threshold=-18, ratio=2, knee=6, attack=10, release=120, makeup=2)
tadd("Vocal | Smooth Rider", "Vocal", threshold=-22, ratio=3, knee=10, attack=6, release=160, makeup=4, props={"autoRelease": 1})
tadd("Vocal | Upfront Rap", "FET", threshold=-20, ratio=6, knee=2, attack=0.8, release=70, makeup=6, drive=3, props={"character": 1})
tadd("Vocal | Silky Opto", "Opto", threshold=-24, ratio=4, knee=6, attack=12, release=300, makeup=5)
tadd("Vocal | Level Then Catch", "Leveler", threshold=-28, ratio=2.5, knee=12, attack=40, release=900, makeup=4)
tadd("Drums | Kick Snap", "Punch", threshold=-16, ratio=4, knee=3, attack=12, release=80, makeup=3)
tadd("Drums | Snare Body", "Punch", threshold=-20, ratio=5, knee=4, attack=6, release=110, makeup=4, drive=2, props={"character": 1})
tadd("Drums | Bus Glue", "Glue", threshold=-14, ratio=3, knee=6, attack=10, release=200, makeup=2, props={"autoRelease": 1}, key=[(1, "lowcut", 90, 0, 0.707, 12)])
tadd("Drums | Room Smash", "Smash", threshold=-30, ratio=20, knee=0, attack=0.2, release=60, makeup=10, drive=6, mix=60, props={"character": 1})
tadd("Bass | Even Notes", "Bus", threshold=-20, ratio=4, knee=6, attack=15, release=150, makeup=4)
tadd("Bass | 808 Hold", "Clean", threshold=-14, ratio=3, knee=6, attack=25, release=250, hold=20, makeup=2)
tadd("Guitar | Acoustic Even", "Opto", threshold=-22, ratio=3, knee=8, attack=10, release=250, makeup=4)
tadd("Guitar | Electric Sustain", "FET", threshold=-26, ratio=8, knee=2, attack=2, release=200, makeup=8, drive=2, props={"character": 1})
tadd("Keys | Piano Gentle", "Clean", threshold=-20, ratio=2, knee=10, attack=20, release=200, makeup=2)
tadd("Mix Bus | Glue 2 dB", "Glue", threshold=-12, ratio=2, knee=8, attack=30, release=300, makeup=1.5, props={"autoRelease": 1}, key=[(1, "lowcut", 80, 0, 0.707, 12)])
tadd("Mix Bus | Warm Mu", "Variable Mu", threshold=-16, ratio=4, knee=6, attack=20, release=400, makeup=2, drive=2, props={"character": 1})
tadd("Master | Transparent 1 dB", "Mastering", threshold=-10, ratio=1.5, knee=12, attack=30, release=400, makeup=1, props={"autoRelease": 1})
tadd("Master | Peak Catcher", "Transparent Peak", threshold=-6, ratio=10, knee=1, attack=0.5, release=80, makeup=1, lookahead=5)
tadd("Podcast | Even Voice", "Vocal", threshold=-28, ratio=4, knee=8, attack=5, release=200, makeup=8, props={"autoRelease": 1, "autoGain": 1})
tadd("Parallel | New York Style Blend", "Smash", threshold=-32, ratio=12, knee=0, attack=1, release=90, makeup=10, mix=40)
# Advanced
tadd("Pump | Quarter-Note Duck (tempo)", "Pump", adv=True, threshold=-30, ratio=20, knee=0, attack=1, release=180, makeup=0, props={"keySource": 3, "tempoDiv": 2})
tadd("Pump | MIDI Kick Duck", "Pump", adv=True, threshold=-30, ratio=20, knee=0, attack=1, release=160, props={"keySource": 2})
tadd("Pump | Sidechain From Kick Track", "Pump", adv=True, threshold=-28, ratio=8, knee=2, attack=1, release=140, props={"keySource": 1}, key=[(1, "highcut", 150, 0, 0.707, 24)])
tadd("Mid-Side | Tame Wide Highs", "Clean", adv=True, threshold=-24, ratio=3, knee=6, attack=5, release=120, props={"routing": 2}, key=[(1, "lowcut", 2000, 0, 0.707, 12)])
tadd("Mid-Side | Center Focus", "Bus", adv=True, threshold=-18, ratio=2.5, knee=6, attack=15, release=200, props={"routing": 1})
tadd("Upward | Bring Up the Detail", "Upward", adv=True, threshold=-30, ratio=2, knee=6, range=12, attack=5, release=200)
tadd("Upward | Quiet Room Lift", "Upward", adv=True, threshold=-40, ratio=3, knee=10, range=18, attack=10, release=300)
tadd("Colour | Iron Thickener", "Bus", adv=True, threshold=-10, ratio=1.5, knee=10, attack=20, release=200, drive=8, props={"character": 4, "oversample": 3})
tadd("Colour | Bright Edge Pre", "FET", adv=True, threshold=-18, ratio=4, knee=2, attack=2, release=90, drive=6, props={"character": 3, "characterPost": 0, "oversample": 3})
tadd("De-Ess | Broadband Ess Control", "Clean", adv=True, threshold=-30, ratio=6, knee=3, attack=0.5, release=60, key=[(1, "bandpass", 6500, 0, 1.5, 24)])
tadd("Extreme | Double Depth 200%", "Glue", adv=True, threshold=-20, ratio=3, knee=6, attack=10, release=200, mix=200, makeup=6)
tadd("Extreme | Brick 100:1", "Transparent Peak", adv=True, threshold=-12, ratio=100, knee=0, attack=0.1, release=50, lookahead=5, makeup=6)

def torque_cpp():
    rows = []
    for name, adv, style, v, props, key in T:
        vals = [(k, x) for k, x in v.items()]
        pr = {"style": STY[style]}; pr.update(props)
        for (b, shape, f, gdb, q, slope) in key:
            vals += [(f"k{b}Freq", f), (f"k{b}Gain", gdb), (f"k{b}Q", q)]
            pr[f"k{b}On"] = 1; pr[f"k{b}Shape"] = SHAPES[shape]; pr[f"k{b}Slope"] = slope
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in vals)
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in pr.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_torque()   // Torque Comp\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"


# ---------------------------------------------------------------- Summit Limiter
ALG = {n: i for i, n in enumerate(["Transparent","Punch","Dense","Smooth","Aggressive","Wide","Bus","Safe","Modern","Clipper"])}
S = []
def sadd(name, alg, adv=False, props=None, **v): S.append((name, adv, alg, v, props or {}))
sadd("Start | Transparent -1 dBTP", "Transparent", input=0, ceiling=-1.0, release=100, lookahead=2)
sadd("Master | Streaming Ready", "Modern", input=4, ceiling=-1.0, release=100, lookahead=2, props={"target": 0})
sadd("Master | Gentle and Open", "Smooth", input=2, ceiling=-1.0, release=120, lookahead=3)
sadd("Master | Loud Pop", "Modern", input=7, ceiling=-0.8, release=90, lookahead=2, props={"target": 6})
sadd("Master | Hip-Hop Punch", "Punch", input=6, ceiling=-0.8, release=100, lookahead=1.5)
sadd("Master | Trap and Drill", "Aggressive", input=8, ceiling=-0.5, release=110, lookahead=1)
sadd("Master | EDM Dense", "Dense", input=8, ceiling=-0.5, release=90, lookahead=1.5, props={"target": 5})
sadd("Master | Acoustic and Jazz", "Smooth", input=3, ceiling=-1.0, release=140, lookahead=4)
sadd("Master | Wide Mix Keeper", "Wide", input=4, ceiling=-1.0, release=100, lookahead=2)
sadd("Bus | Drum Bus Peaks", "Bus", input=3, ceiling=-3.0, release=80, lookahead=1)
sadd("Bus | Vocal Bus Catch", "Safe", input=2, ceiling=-3.0, release=100, lookahead=2)
sadd("Safety | Last in the Chain", "Safe", input=0, ceiling=-0.3, release=100, lookahead=2)
sadd("Podcast | -16 LUFS Voice", "Transparent", input=6, ceiling=-1.0, release=100, lookahead=3, props={"target": 2})
sadd("Broadcast | EU -23", "Transparent", input=0, ceiling=-1.0, release=100, lookahead=3, props={"target": 3})
sadd("Broadcast | US -24", "Transparent", input=0, ceiling=-2.0, release=100, lookahead=3, props={"target": 4})
sadd("Export | 16-bit with Shaped Dither", "Transparent", input=0, ceiling=-1.0, release=100, lookahead=2, props={"ditherBits": 1, "noiseShape": 2})
# Advanced
sadd("Clip | Clipper Loudness", "Clipper", adv=True, input=6, ceiling=-0.5, release=100, lookahead=0.5)
sadd("Clip | Clip Then Limit (8x)", "Dense", adv=True, input=9, ceiling=-0.3, release=80, lookahead=1, props={"oversample": 3})
sadd("Linking | Transients Free, Release Linked", "Transparent", adv=True, input=4, ceiling=-1.0, release=100, lookahead=2, transientLink=10, releaseLink=100, props={"linkOwn": 0})
sadd("Linking | Fully Independent", "Transparent", adv=True, input=4, ceiling=-1.0, release=100, lookahead=2, transientLink=0, releaseLink=0, props={"linkOwn": 0})
sadd("Character | Fast Breathing Release", "Aggressive", adv=True, input=6, ceiling=-1.0, release=40, lookahead=1)
sadd("Character | Slow Glue Release", "Transparent", adv=True, input=5, ceiling=-1.0, release=300, lookahead=4)
sadd("Studio | Sample Peak Only (no guard)", "Transparent", adv=True, input=3, ceiling=-0.1, release=100, lookahead=2, props={"truePeak": 0})
sadd("Studio | Bit-Exact Below Ceiling", "Transparent", adv=True, input=0, ceiling=-0.1, release=100, lookahead=2, props={"bandLimit": 0, "dcFilter": 0})

def summit_cpp():
    rows = []
    for name, adv, alg, v, props in S:
        pr = {"algorithm": ALG[alg]}; pr.update(props)
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in v.items())
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in pr.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_summit()   // Summit Limiter\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Atrium Verb
SPC = {n: i for i, n in enumerate(["Concert Hall","Plate","Grand Hall","Chamber","Studio Room","Booth","Cathedral","Ambience","Vintage Digital","Bloom","Dark Space","Gate"])}
AV = []
def aadd(name, space, adv=False, props=None, **v): AV.append((name, adv, space, v, props or {}))
aadd("Start | Concert Hall", "Concert Hall", decay=2.5, size=60, predelay=10, mix=30)
aadd("Vocal | Bright Plate", "Plate", decay=1.8, size=55, predelay=30, diffusion=95, character=50, highMult=0.8, lowCut=180, mix=22)
aadd("Vocal | Ballad Hall", "Concert Hall", decay=3.2, size=70, predelay=60, lowMult=1.1, highMult=0.55, lowCut=150, highCut=11000, mix=24)
aadd("Vocal | Rap Room", "Studio Room", decay=0.7, size=35, predelay=12, lowCut=250, mix=14)
aadd("Vocal | Ducked Long Tail", "Grand Hall", decay=4.5, size=80, predelay=40, duck=10, duckThresh=-30, duckRelease=350, lowCut=200, mix=30)
aadd("Drums | Snare Plate", "Plate", decay=1.3, size=50, predelay=0, diffusion=100, highMult=0.9, lowCut=220, mix=25)
aadd("Drums | Room Glue", "Chamber", decay=0.9, size=45, predelay=4, early=130, late=80, lowCut=90, mix=18)
aadd("Drums | Gated Snare", "Gate", decay=2.0, size=50, predelay=0, early=120, lowCut=200, mix=35)
aadd("Drums | Big Tom Hall", "Concert Hall", decay=2.2, size=75, predelay=8, lowMult=1.0, highMult=0.5, mix=22)
aadd("Keys | Warm Chamber", "Chamber", decay=1.9, size=60, predelay=18, lowMult=1.3, highMult=0.5, highCut=9000, mix=26)
aadd("Keys | Piano Hall", "Concert Hall", decay=2.8, size=70, predelay=22, lowMult=1.15, highMult=0.6, mix=24)
aadd("Guitar | Vintage Shimmer", "Vintage Digital", decay=3.0, size=65, predelay=35, character=80, mix=28)
aadd("Guitar | Small Amp Room", "Booth", decay=0.45, size=40, predelay=3, early=140, mix=18)
aadd("Pads | Endless Bloom", "Bloom", decay=9.0, size=90, predelay=20, character=60, lowMult=1.2, highMult=0.7, lowCut=120, mix=45)
aadd("Pads | Cathedral Wash", "Cathedral", decay=7.5, size=85, predelay=45, lowMult=1.3, highMult=0.45, mix=38)
aadd("Mix | Subtle Ambience", "Ambience", decay=0.6, size=40, predelay=0, early=120, late=60, mix=12)
aadd("Mix | Dark Space", "Dark Space", decay=3.5, size=75, predelay=25, mix=25)
aadd("Mix | Bus Depth", "Studio Room", decay=1.1, size=55, predelay=10, lowCut=200, highCut=9000, mix=10)
aadd("Strings | Grand Hall", "Grand Hall", decay=3.6, size=85, predelay=28, lowMult=1.2, highMult=0.55, mix=28)
aadd("Dialogue | Small Office", "Booth", decay=0.35, size=30, predelay=2, early=110, late=60, lowCut=120, mix=15)
# Advanced
aadd("Sound Design | Freeze Pad", "Bloom", adv=True, decay=12.0, size=100, predelay=0, character=70, mix=60, props={"freeze": 1})
aadd("Sound Design | Reverse-ish Swell", "Bloom", adv=True, decay=4.0, size=95, predelay=120, diffusion=100, early=0, mix=55)
aadd("Tempo | Synced 1/8 Predelay Plate", "Plate", adv=True, decay=1.6, size=55, mix=25, props={"predelaySync": 4})
aadd("Tempo | Synced 1/16 Hall", "Concert Hall", adv=True, decay=2.4, size=65, mix=25, props={"predelaySync": 3})
aadd("Shape | Bass Stays Short", "Concert Hall", adv=True, decay=3.0, size=70, predelay=20, lowMult=0.4, lowX=300, highMult=0.7, mix=26)
aadd("Shape | Air Rings On", "Plate", adv=True, decay=2.2, size=55, predelay=20, highMult=1.4, highX=6000, lowCut=250, mix=20)
aadd("Width | Ultra Wide Hall", "Grand Hall", adv=True, decay=3.4, size=85, predelay=30, width=200, mix=26)
aadd("Width | Mono Room", "Chamber", adv=True, decay=1.2, size=50, predelay=10, width=0, mix=20)
aadd("Character | Still Tail (no motion)", "Concert Hall", adv=True, decay=2.5, size=60, predelay=10, character=0, mix=28)
aadd("Character | Chorused 80s Hall", "Vintage Digital", adv=True, decay=2.8, size=70, predelay=15, character=100, mix=30)

def atrium_cpp():
    rows = []
    for name, adv, sp, v, props in AV:
        pr = {"space": SPC[sp]}; pr.update(props)
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in v.items())
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in pr.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_atrium()   // Atrium Verb\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Apex Multiband 2
MODE = {"comp": 0, "up": 1, "exp": 2, "expup": 3}
PLACE = {"st": 0, "l": 1, "r": 2, "m": 3, "s": 4}
MB = []
def bnd(thr=-20, ratio=2, rng=12, knee=6, att=10, rel=150, gain=0, mode="comp", place="st", key=0):
    return dict(thr=thr, ratio=ratio, range=rng, knee=knee, att=att, rel=rel, gain=gain, mode=MODE[mode], place=PLACE[place], key=key)
def madd(name, xo, bands, adv=False, phase=0, look=0, det=0, inp=0, out=0, mix=100):
    MB.append((name, adv, xo, bands, phase, look, det, inp, out, mix))
madd("Start | Four Bands, Gentle", [120, 500, 2000, 6000], [bnd(-24, 2), bnd(-24, 2), bnd(-24, 2), bnd(-24, 2)])
madd("Master | Glue the Mix", [120, 600, 3000, 8000], [bnd(-22, 1.8, 6, 10, 30, 200), bnd(-24, 1.6, 4, 10, 20, 160), bnd(-26, 1.5, 4, 10, 15, 120), bnd(-26, 1.5, 4, 10, 10, 100)], look=2)
madd("Master | Tame the Low End", [90, 250, 2000, 8000], [bnd(-26, 3, 6, 6, 30, 180), bnd(-24, 2.5, 6, 6, 20, 150), bnd(0, 1, 0), bnd(0, 1, 0)], look=3)
madd("Master | Smooth Harsh Highs", [200, 2500, 5000, 9000], [bnd(0, 1, 0), bnd(0, 1, 0), bnd(-30, 3, 5, 8, 2, 80), bnd(-30, 2.5, 4, 8, 2, 80)], look=2)
madd("Master | Open the Top (upward)", [150, 1000, 6000, 12000], [bnd(0, 1, 0), bnd(0, 1, 0), bnd(-32, 2, 4, 8, 15, 200, mode="up"), bnd(-36, 2, 5, 8, 15, 200, mode="up")])
madd("Master | Wide Side Air", [200, 3000, 8000, 12000], [bnd(0, 1, 0), bnd(0, 1, 0), bnd(-36, 2, 3, 8, 20, 200, mode="up", place="s"), bnd(-36, 2, 3, 8, 20, 200, mode="up", place="s")])
madd("Master | Mono the Lows", [150, 500, 2000, 6000], [bnd(-60, 20, 24, 0, 1, 50, place="s"), bnd(0, 1, 0), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Mix | Bass Steady", [80, 250, 1000, 6000], [bnd(-24, 4, 8, 6, 15, 120), bnd(-22, 3, 6, 6, 10, 100), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Mix | Vocal Even", [180, 800, 3500, 8000], [bnd(0, 1, 0), bnd(-28, 2.5, 6, 8, 8, 120), bnd(-28, 3, 6, 8, 5, 100), bnd(-30, 3, 6, 8, 2, 80)])
madd("Mix | De-Box a Vocal", [180, 300, 600, 3000], [bnd(0, 1, 0), bnd(-30, 3, 6, 6, 10, 120), bnd(-30, 3, 6, 6, 10, 120), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Mix | Acoustic Guitar Boom", [120, 300, 2000, 6000], [bnd(0, 1, 0), bnd(-28, 4, 8, 6, 8, 100), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Drums | Drum Bus Punch", [100, 400, 3000, 8000], [bnd(-20, 3, 6, 6, 25, 100), bnd(-22, 2, 4, 6, 20, 90), bnd(-24, 2, 4, 6, 15, 80), bnd(-30, 2, 4, 6, 5, 70, mode="expup")])
madd("Drums | Tighten the Room", [150, 600, 3000, 8000], [bnd(-40, 2, 10, 6, 1, 120, mode="exp"), bnd(-40, 2, 10, 6, 1, 120, mode="exp"), bnd(-40, 2, 8, 6, 1, 100, mode="exp"), bnd(-40, 2, 8, 6, 1, 100, mode="exp")])
madd("Drums | Snare Crack Lift", [200, 1000, 3000, 8000], [bnd(0, 1, 0), bnd(0, 1, 0), bnd(-28, 3, 6, 6, 1, 60, mode="expup"), bnd(0, 1, 0)])
madd("Beat | Kick Ducks the Bass (side-chain)", [120, 500, 2000, 6000], [bnd(-30, 10, 8, 0, 1, 120, key=1), bnd(0, 1, 0), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Beat | 808 Control", [60, 160, 800, 5000], [bnd(-20, 4, 8, 6, 20, 150), bnd(-22, 3, 6, 6, 15, 120), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Podcast | Even Voice", [150, 700, 3000, 7000], [bnd(-36, 2, 10, 6, 5, 200, mode="exp"), bnd(-24, 3, 8, 8, 10, 150), bnd(-24, 3, 8, 8, 5, 120), bnd(-30, 4, 8, 8, 1, 60)])
madd("Podcast | Lift Quiet Speech", [150, 700, 3000, 7000], [bnd(0, 1, 0), bnd(-34, 2, 8, 8, 10, 250, mode="up"), bnd(-34, 2, 8, 8, 10, 250, mode="up"), bnd(0, 1, 0)])
madd("Clean | Hum and Rumble Down", [60, 200, 2000, 8000], [bnd(-50, 4, 18, 6, 5, 200, mode="exp"), bnd(0, 1, 0), bnd(0, 1, 0), bnd(0, 1, 0)])
madd("Clean | Hiss Down", [200, 2000, 6000, 10000], [bnd(0, 1, 0), bnd(0, 1, 0), bnd(-56, 3, 12, 6, 5, 200, mode="exp"), bnd(-56, 3, 12, 6, 5, 200, mode="exp")])
# Advanced
madd("Linear | Transparent Master Six", [80, 250, 800, 3000, 9000], [bnd(-24, 1.5, 4, 10, 20, 200)] * 6, adv=True, phase=1, look=5)
madd("Linear | Surgical Two-Band", [180], [bnd(-26, 3, 6, 6, 10, 150), bnd(-26, 2, 4, 6, 5, 100)], adv=True, phase=1)
madd("Stereo | Left / Right Independent Lows", [150, 1000, 5000, 10000], [bnd(-24, 3, 6, 6, 15, 120, place="l"), bnd(-24, 3, 6, 6, 15, 120, place="r"), bnd(0, 1, 0), bnd(0, 1, 0)], adv=True)
madd("Stereo | Mid Focus", [200, 1000, 5000, 10000], [bnd(0, 1, 0), bnd(-26, 2.5, 6, 8, 10, 120, place="m"), bnd(-26, 2.5, 6, 8, 10, 120, place="m"), bnd(0, 1, 0)], adv=True)
madd("Character | Over-the-Top Squash", [100, 500, 3000, 8000], [bnd(-40, 10, 18, 3, 1, 60, gain=6), bnd(-40, 10, 18, 3, 1, 60, gain=6), bnd(-40, 10, 18, 3, 1, 60, gain=6), bnd(-40, 10, 18, 3, 1, 60, gain=4)], adv=True, mix=50)
madd("Character | Parallel Upward Density", [120, 800, 4000, 10000], [bnd(-40, 3, 10, 10, 10, 200, mode="up")] * 4, adv=True, mix=60)
madd("Transient | Snap Every Band", [150, 800, 4000, 10000], [bnd(-30, 3, 6, 6, 1, 50, mode="expup")] * 4, adv=True, det=3)
madd("Detector | Peak Catcher 5 ms Look", [120, 500, 2000, 6000], [bnd(-12, 8, 10, 2, 0.5, 60)] * 4, adv=True, look=5, det=3)

def mb2_cpp():
    rows = []
    for name, adv, xo, bands, phase, look, det, inp, out, mix in MB:
        vals = [("input", inp), ("output", out), ("mix", mix), ("lookahead", look)]
        for i, x in enumerate(xo): vals.append((f"x{i+1}", x))
        props = [("bands", len(bands)), ("phase", phase), ("detector", det)]
        for i, b in enumerate(bands):
            n = i + 1
            vals += [(f"b{n}thr", b["thr"]), (f"b{n}ratio", b["ratio"]), (f"b{n}range", b["range"]), (f"b{n}knee", b["knee"]), (f"b{n}att", b["att"]), (f"b{n}rel", b["rel"]), (f"b{n}gain", b["gain"])]
            props += [(f"mode{n}", b["mode"]), (f"place{n}", b["place"]), (f"key{n}", b["key"])]
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in vals)
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props)
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_mb2()   // Apex Multiband 2\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Sibyl De-Ess
SB = []
def sbadd(name, adv=False, shape=0, detect=1, stereo=0, track=0, side=0, **v): SB.append((name, adv, dict(shape=shape, detect=detect, stereo=stereo, track=track, sidechain=side), v))
sbadd("Start | Natural Voice", threshold=-12, range=8, knee=6, release=60, lookahead=2, low=5000, high=10000)
sbadd("Vocal | Female Pop", threshold=-10, range=9, knee=6, release=50, lookahead=2, low=6000, high=11000)
sbadd("Vocal | Male Pop", threshold=-12, range=8, knee=6, release=60, lookahead=2, low=4500, high=9000)
sbadd("Vocal | Rap, Close Mic", threshold=-14, range=10, knee=4, release=45, lookahead=3, low=5000, high=10000)
sbadd("Vocal | Breathy Ballad", threshold=-16, range=6, knee=9, release=80, lookahead=2, low=5500, high=12000)
sbadd("Vocal | Bright Condenser Tamer", threshold=-12, range=10, knee=6, release=60, lookahead=2, low=6500, high=13000, track=1)
sbadd("Vocal | Hunt the S (track)", threshold=-12, range=9, knee=6, release=55, lookahead=2, low=4000, high=12000, track=1)
sbadd("Vocal | Gentle Polish", threshold=-8, range=4, knee=12, release=70, lookahead=2, low=5000, high=10000)
sbadd("Vocal | Heavy Lisp Fix", threshold=-18, range=14, knee=3, release=40, lookahead=4, low=4500, high=10000)
sbadd("Backing | Stack of Voices", threshold=-14, range=10, knee=6, release=70, lookahead=2, low=5000, high=11000)
sbadd("Podcast | Speech", threshold=-14, range=8, knee=6, release=60, lookahead=3, low=4500, high=9000)
sbadd("Podcast | Phone Bright", threshold=-12, range=6, knee=6, release=60, lookahead=2, low=3000, high=6500)
sbadd("Mix | Cymbal Sizzle", threshold=-28, range=6, knee=6, release=80, lookahead=2, low=7000, high=14000, detect=0)
sbadd("Mix | Acoustic Guitar Squeaks", threshold=-30, range=8, knee=4, release=60, lookahead=2, low=2500, high=6000, detect=0)
sbadd("Master | Harsh Top, Whole Mix", threshold=-30, range=3, knee=9, release=90, lookahead=2, low=5500, high=11000, detect=0)
sbadd("Master | Side Sibilance Only", threshold=-30, range=5, knee=9, release=80, lookahead=2, low=5000, high=11000, detect=0, stereo=2)
# Advanced
sbadd("Wide | Whole Voice Ducks on S", adv=True, threshold=-12, range=6, knee=6, release=50, lookahead=2, shape=1)
sbadd("Wide | Old-School Broadband", adv=True, threshold=-14, range=10, knee=3, release=40, lookahead=0, shape=1, detect=0)
sbadd("Detect | Absolute, Steady Level", adv=True, threshold=-34, range=8, knee=6, release=60, lookahead=2, detect=0)
sbadd("Detect | Fast Release Consonants", adv=True, threshold=-12, range=10, knee=4, release=15, lookahead=3)
sbadd("Detect | Max Lookahead, No Pre-Hiss", adv=True, threshold=-12, range=10, knee=6, release=60, lookahead=10)
sbadd("Stereo | Mid Only (centre vocal)", adv=True, threshold=-12, range=8, knee=6, release=60, lookahead=2, stereo=1)
sbadd("Key | Side-Chain From Lead Vocal", adv=True, threshold=-14, range=8, knee=6, release=60, lookahead=2, side=1, detect=0)
sbadd("Fix | Very Narrow Whistle", adv=True, threshold=-18, range=12, knee=3, release=40, lookahead=2, low=7500, high=8800, track=1)

def sibyl_cpp():
    rows = []
    for name, adv, props, v in SB:
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in v.items())
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_sibyl()   // Sibyl De-Ess\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Portcullis Gate
PG = []
def padd(name, adv=False, mode=0, key=0, curve=1, flip=0, velocity=0, listen=0, **v): PG.append((name, adv, dict(mode=mode, key=key, curve=curve, flip=flip, velocity=velocity, listenKey=listen), v))
padd("Start | Clean Gate", threshold=-40, hysteresis=6, range=60, attack=0.5, hold=30, release=120)
padd("Drums | Kick Tight", threshold=-28, hysteresis=6, range=40, attack=0.1, hold=20, release=80, lookahead=1, keyHp=30, keyLp=300, curve=2)
padd("Drums | Snare, No Hi-Hat Bleed", threshold=-26, hysteresis=8, range=30, attack=0.1, hold=40, release=150, lookahead=1.5, keyHp=150, keyLp=6000, curve=2)
padd("Drums | Toms Ring Control", threshold=-32, hysteresis=10, range=40, attack=0.2, hold=80, release=250, lookahead=2, keyHp=60, keyLp=2000)
padd("Drums | Overheads Gentle", threshold=-40, hysteresis=6, range=10, attack=1, hold=50, release=300, mode=1, ratio=2)
padd("Drums | Room Gated (80s)", threshold=-24, hysteresis=4, range=80, attack=0.1, hold=180, release=20, curve=0)
padd("Vocal | Breaths Down", threshold=-42, hysteresis=6, range=12, attack=2, hold=60, release=200, mode=1, ratio=2)
padd("Vocal | Headphone Bleed", threshold=-38, hysteresis=8, range=24, attack=1, hold=100, release=250, lookahead=2, keyHp=100)
padd("Podcast | Room Noise Between Words", threshold=-46, hysteresis=8, range=18, attack=2, hold=120, release=300, mode=1, ratio=3)
padd("Guitar | Amp Hiss Off", threshold=-50, hysteresis=10, range=40, attack=1, hold=60, release=200)
padd("Guitar | Metal Chug Chop", threshold=-36, hysteresis=4, range=80, attack=0.05, hold=5, release=20, lookahead=1, curve=2)
padd("Bass | DI Clean", threshold=-48, hysteresis=8, range=30, attack=1, hold=50, release=180, keyLp=2000)
padd("Mix | Gentle Expander", threshold=-45, hysteresis=4, range=15, attack=3, hold=40, release=250, mode=1, ratio=1.5)
padd("Duck | Music Under Voice", threshold=-36, hysteresis=4, range=10, attack=10, hold=200, release=600, mode=2, key=1)
padd("Duck | Pad Pump (side-chain kick)", threshold=-30, hysteresis=2, range=18, attack=1, hold=0, release=160, mode=2, key=1, curve=2)
padd("Clean | Noise Floor Polish", threshold=-60, hysteresis=6, range=20, attack=2, hold=50, release=400, mode=1, ratio=2)
# Advanced
padd("MIDI | Trance Gate (play notes)", adv=True, range=80, attack=0.5, hold=0, release=15, key=2, curve=0)
padd("MIDI | Velocity Gate", adv=True, range=80, attack=1, hold=0, release=40, key=2, velocity=1)
padd("Side-Chain | Kick Opens the Bass", adv=True, threshold=-30, hysteresis=4, range=40, attack=0.2, hold=20, release=90, key=1, curve=2)
padd("Key | Hi-Hat Only Opens (HP 6k)", adv=True, threshold=-34, hysteresis=6, range=40, attack=0.1, hold=20, release=60, keyHp=6000, curve=2)
padd("Flip | Hear Only the Bleed", adv=True, threshold=-30, hysteresis=6, range=60, attack=0.5, hold=30, release=120, flip=1)
padd("Flip | Isolate the Room Tail", adv=True, threshold=-24, hysteresis=6, range=80, attack=0.1, hold=10, release=40, flip=1)
padd("Shape | Slow Swell In", adv=True, threshold=-40, hysteresis=6, range=60, attack=60, hold=30, release=400, curve=1)
padd("Listen | Tune the Key Filter", adv=True, threshold=-30, hysteresis=6, range=60, attack=0.5, hold=30, release=120, keyHp=200, keyLp=5000, listen=1)

def portcullis_cpp():
    rows = []
    for name, adv, props, v in PG:
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in v.items())
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_portcullis()   // Portcullis Gate\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Kiln
KM = {n: i for i, n in enumerate(["Tape Preamp","British Channel","Germanium Class-A","Triode Box","Pentode Box","Transformer Iron","Op-Amp Rail","Diode Fuzz","Tape Hysteresis","Broadcast Limiter Amp"])}
KL = []
def kb(model, drive=6, bias=0, dyn=30, mix=100, out=0): return dict(model=KM[model], drive=drive, bias=bias, dyn=dyn, mix=mix, out=out)
def kadd(name, xo, bands, adv=False, os=2, lin=0, auto=1, inp=0, out=0, mix=100, lfo=(0, 1.0), env=(10, 150), slots=()):
    KL.append((name, adv, xo, bands, os, lin, auto, inp, out, mix, lfo, env, slots))
kadd("Start | Warm Three-Band", [200, 2500], [kb("Transformer Iron", 4), kb("Triode Box", 4), kb("Tape Hysteresis", 3)])
kadd("Mix | Glue and Glow", [180, 3000], [kb("Transformer Iron", 3, dyn=40), kb("Tape Preamp", 4, dyn=40), kb("Tape Preamp", 2, dyn=40)], mix=70)
kadd("Mix | Tape Bus", [120, 4000], [kb("Tape Hysteresis", 6), kb("Tape Hysteresis", 4), kb("Tape Hysteresis", 3)])
kadd("Drums | Fat Low, Clean Top", [150, 5000], [kb("Germanium Class-A", 12), kb("British Channel", 4), kb("Tape Preamp", 0, mix=0)])
kadd("Drums | Crushed Room", [250, 4000], [kb("Diode Fuzz", 18), kb("Pentode Box", 16), kb("Op-Amp Rail", 12)], mix=40)
kadd("Drums | Snare Crack", [300, 2500, 7000], [kb("Tape Preamp", 0, mix=0), kb("Pentode Box", 12), kb("Triode Box", 8), kb("Tape Preamp", 0, mix=0)])
kadd("Bass | Growl Up Top Only", [200], [kb("Tape Preamp", 0, mix=0), kb("Diode Fuzz", 20, mix=80)])
kadd("Bass | Warm Weight", [100, 800], [kb("Transformer Iron", 8), kb("Triode Box", 6), kb("Tape Preamp", 0, mix=0)])
kadd("808 | Audible on Phones", [90], [kb("Tape Preamp", 0, mix=0), kb("Pentode Box", 18)])
kadd("Vocal | Presence Heat", [300, 2500, 8000], [kb("Tape Preamp", 0, mix=0), kb("Tape Preamp", 3), kb("Triode Box", 8, mix=70), kb("Tape Preamp", 2)])
kadd("Vocal | Radio Edge", [400, 4000], [kb("Tape Preamp", 0, mix=0), kb("Broadcast Limiter Amp", 12), kb("Op-Amp Rail", 6, mix=50)])
kadd("Guitar | Amp-Like Mids", [250, 3000], [kb("Tape Preamp", 2), kb("Pentode Box", 18), kb("Triode Box", 6, mix=60)])
kadd("Keys | Electric Piano Bark", [300, 2000], [kb("Tape Preamp", 2), kb("Triode Box", 14), kb("Tape Preamp", 3)])
kadd("Synth | Bright Fizz", [800, 5000], [kb("Tape Preamp", 0, mix=0), kb("Diode Fuzz", 12, mix=60), kb("Op-Amp Rail", 10, mix=50)])
kadd("Master | Gentle Analogue", [150, 3000], [kb("Transformer Iron", 2, dyn=50), kb("Tape Preamp", 2, dyn=50), kb("Tape Preamp", 1, dyn=50)], mix=60, os=3, lin=1)
kadd("Master | Loud and Thick", [120, 2000, 8000], [kb("Transformer Iron", 6), kb("British Channel", 5), kb("Tape Hysteresis", 4), kb("Tape Preamp", 3)], os=3, lin=1)
kadd("Lo-Fi | Broken Speaker", [500, 3000], [kb("Diode Fuzz", 24, bias=0.6), kb("Op-Amp Rail", 20), kb("Diode Fuzz", 24, bias=-0.5)], mix=60)
kadd("Lo-Fi | Old Radio", [300, 3500], [kb("Tape Preamp", -12, mix=0, out=-24), kb("Broadcast Limiter Amp", 18), kb("Tape Preamp", -12, mix=0, out=-24)])
# Advanced (modulated)
kadd("Motion | Pulsing Drive (LFO)", [200, 2500], [kb("Transformer Iron", 4), kb("Pentode Box", 10), kb("Triode Box", 4)], adv=True, lfo=(0, 2.0), slots=((1, 2, 0.4),))
kadd("Motion | Random Grit Steps", [300, 3000], [kb("Tape Preamp", 2), kb("Diode Fuzz", 8), kb("Op-Amp Rail", 6)], adv=True, lfo=(3, 4.0), slots=((1, 2, 0.5), (1, 3, 0.3)))
kadd("Motion | Square Gate Grit", [200, 4000], [kb("Tape Preamp", 0), kb("Pentode Box", 12), kb("Tape Preamp", 0)], adv=True, lfo=(2, 1.0), slots=((1, 8, -1.0),))
kadd("Dynamic | Louder Is Dirtier", [250, 3000], [kb("Transformer Iron", 6, mix=0), kb("Pentode Box", 14, mix=0), kb("Triode Box", 8, mix=0)], adv=True, env=(5, 120), slots=((2, 7, 1.0), (2, 8, 1.0), (2, 9, 1.0)))
kadd("Dynamic | Quiet Gets Warm", [200, 3000], [kb("Triode Box", 12), kb("Triode Box", 10), kb("Tape Preamp", 6)], adv=True, env=(20, 300), slots=((2, 13, -0.8),))
kadd("Dynamic | Hits Light the Top", [300, 4000], [kb("Tape Preamp", 0, mix=0), kb("Tape Preamp", 0, mix=0), kb("Op-Amp Rail", 14, mix=0)], adv=True, env=(1, 80), slots=((2, 9, 1.0),))
kadd("Six | Full Spectrum Heat", [80, 250, 800, 3000, 9000], [kb("Transformer Iron", 6), kb("Germanium Class-A", 6), kb("British Channel", 6), kb("Triode Box", 6), kb("Tape Hysteresis", 6), kb("Tape Preamp", 4)], adv=True, os=3)
kadd("Clean | Oversampled 16x Polish", [200, 4000], [kb("Tape Preamp", 3), kb("Tape Preamp", 3), kb("Tape Preamp", 3)], adv=True, os=4, lin=1, mix=50)

def kiln_cpp():
    rows = []
    for name, adv, xo, bands, os_, lin, auto, inp, out, mix, lfo, env, slots in KL:
        vals = [("input", inp), ("output", out), ("mix", mix), ("lfoRate", lfo[1]), ("envAttack", env[0]), ("envRelease", env[1])]
        for i, x in enumerate(xo): vals.append((f"x{i+1}", x))
        props = [("bands", len(bands)), ("oversample", os_), ("linearOs", lin), ("autoGain", auto), ("lfoShape", lfo[0])]
        for i, b in enumerate(bands):
            n = i + 1
            vals += [(f"b{n}drive", b["drive"]), (f"b{n}bias", b["bias"]), (f"b{n}dyn", b["dyn"]), (f"b{n}mix", b["mix"]), (f"b{n}out", b["out"])]
            props += [(f"model{n}", b["model"])]
        for i, (src, dst, amt) in enumerate(slots):
            vals.append((f"slot{i+1}", amt)); props += [(f"src{i+1}", src), (f"dst{i+1}", dst)]
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in vals)
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props)
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_kiln()   // Kiln\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Spindle Delay
CH = {"digital": 0, "tape": 1, "analog": 2, "diffuse": 3}
RT = {"normal": 0, "pingpong": 1, "cross": 2}
SD = []
CHDEF = {0: (0, 0, 0), 1: (35, 25, 0), 2: (15, 5, 10), 3: (10, 5, 80)}
def sdadd(name, adv=False, ch="tape", rt="normal", sync=3, syncR=None, link=1, freeze=0, **v):
    w, f, d = CHDEF[CH[ch]]
    vals = dict(wow=w, flutter=f, diffusion=d); vals.update(v)
    SD.append((name, adv, dict(character=CH[ch], routing=RT[rt], syncL=sync, syncR=sync if syncR is None else syncR, linkLR=link, freeze=freeze), vals))
sdadd("Start | Tape Eighth", feedback=40, mix=30)
sdadd("Vocal | Slapback", ch="tape", sync=0, timeL=110, feedback=0, mix=25, highCut=6000)
sdadd("Vocal | Quarter Throw", ch="tape", sync=4, feedback=35, mix=22, duck=10, lowCut=200, highCut=6000)
sdadd("Vocal | Dotted Eighth Pop", ch="digital", sync=8, feedback=30, mix=20, duck=8, lowCut=250, highCut=8000)
sdadd("Vocal | Wide Ping-Pong", ch="digital", rt="pingpong", sync=3, feedback=45, mix=22, duck=6, lowCut=200, highCut=7000)
sdadd("Vocal | Ambient Wash", ch="diffuse", sync=4, feedback=55, mix=30, lowCut=200, highCut=6000)
sdadd("Guitar | Analog Warm Repeats", ch="analog", sync=8, feedback=50, mix=30, drive=40)
sdadd("Guitar | Dub Spring-Free Echo", ch="tape", sync=4, feedback=75, mix=35, drive=40, lowCut=300, highCut=3500)
sdadd("Guitar | Doubler (no repeats)", ch="digital", sync=0, link=0, timeL=18, timeR=27, feedback=0, mix=40, width=180)
sdadd("Keys | Stereo Spread", ch="digital", sync=0, link=0, timeL=250, timeR=375, feedback=35, mix=22, rt="cross", crossFeed=40)
sdadd("Drums | Snare Tail Quarter", ch="tape", sync=4, feedback=30, mix=18, lowCut=300, highCut=5000, duck=6)
sdadd("Drums | Tight Room Echo", ch="analog", sync=0, timeL=70, feedback=20, mix=15, highCut=5000)
sdadd("Synth | Triplet Bounce", ch="digital", rt="pingpong", sync=11, feedback=45, mix=28)
sdadd("Synth | Long Analog Trail", ch="analog", sync=5, feedback=65, mix=28, drive=30, highCut=4000)
sdadd("FX | Telephone Echo", ch="digital", sync=3, feedback=50, mix=40, lowCut=800, highCut=2500)
sdadd("FX | Lo-Fi Wobble", ch="tape", sync=8, feedback=50, mix=35, wow=90, flutter=60, drive=50, highCut=3000)
sdadd("Mix | Subtle Depth", ch="tape", sync=4, feedback=20, mix=10, lowCut=300, highCut=5000)
sdadd("Mix | Ducked Everything", ch="tape", sync=8, feedback=40, mix=25, duck=14, duckRelease=200)
# Advanced
sdadd("Loop | Freeze Pad", adv=True, ch="diffuse", sync=4, feedback=90, mix=50, freeze=1)
sdadd("Loop | Self-Oscillation Rise", adv=True, ch="tape", sync=3, feedback=105, mix=35, drive=60, highCut=4000)
sdadd("Loop | Runaway Analog", adv=True, ch="analog", sync=0, timeL=60, feedback=100, mix=30, drive=70, highCut=2500)
sdadd("Stereo | Uneven Cross Feed", adv=True, ch="digital", rt="cross", sync=0, link=0, timeL=310, timeR=460, feedback=55, crossFeed=70, mix=25)
sdadd("Stereo | Haas Widener", adv=True, ch="digital", sync=0, link=0, timeL=1, timeR=12, feedback=0, mix=50, width=200)
sdadd("Character | Clean Digital Grid", adv=True, ch="digital", sync=3, feedback=50, mix=30, lowCut=10, highCut=20000)
sdadd("Character | Heavy Diffuse Cloud", adv=True, ch="diffuse", sync=5, feedback=70, mix=40, diffusion=100)
sdadd("Tempo | Half-Bar Dotted", adv=True, ch="tape", sync=9, feedback=45, mix=25)

def spindle_cpp():
    rows = []
    for name, adv, props, v in SD:
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in v.items())
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props.items())
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_spindle()   // Spindle Delay\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

# ---------------------------------------------------------------- Fissure Filter
FT = {"lp": 0, "hp": 1, "bp": 2, "notch": 3, "peak": 4, "comb": 5, "vowel": 6}
FR = {"serial": 0, "parallel": 1, "pairs": 2, "ms": 3}
FI = []
def fs_(t="lp", cut=1000, res=20, drive=0, gain=6, pan=0, mix=100, slope=24, on=1): return dict(t=FT[t], cut=cut, res=res, drive=drive, gain=gain, pan=pan, mix=mix, slope=slope, on=on)
OFF = dict(t=0, cut=1000, res=20, drive=0, gain=6, pan=0, mix=100, slope=24, on=0)
def fadd(name, slots, adv=False, routing="serial", lfo1=(0, 1.0, 0), lfo2=(1, 0.25, 0), env=(5, 120, 0), mods=(), mix=100, out=0):
    sl = list(slots) + [OFF] * (4 - len(slots)); FI.append((name, adv, FR[routing], sl, lfo1, lfo2, env, mods, mix, out))
fadd("Start | Warm Low-Pass", [fs_("lp", 3000, 15)])
fadd("Sweep | Slow LFO Low-Pass", [fs_("lp", 800, 45)], lfo1=(0, 0.25, 0), mods=((1, 1, 0.4),))
fadd("Sweep | Tempo Quarter Wobble", [fs_("lp", 600, 60, slope=24)], lfo1=(0, 1, 5), mods=((1, 1, 0.5),))
fadd("Sweep | Eighth Note Gate Filter", [fs_("lp", 400, 30)], lfo1=(3, 1, 6), mods=((1, 1, 0.6),))
fadd("Sweep | Random Steps (S&H)", [fs_("bp", 1200, 60, slope=12)], lfo1=(4, 4, 7), mods=((1, 1, 0.45),))
fadd("Envelope | Funky Auto-Wah", [fs_("bp", 500, 65, slope=12)], env=(3, 120, 6), mods=((3, 1, 0.55),))
fadd("Envelope | Opens on Hits", [fs_("lp", 400, 35)], env=(2, 200, 0), mods=((3, 1, 0.6),))
fadd("Drums | Telephone Break", [fs_("hp", 400, 20, slope=24), fs_("lp", 3000, 25, slope=24)])
fadd("Drums | Crunchy Band-Pass", [fs_("bp", 1500, 40, drive=12, slope=12)])
fadd("Bass | Squelchy Resonance", [fs_("lp", 350, 75, drive=6, slope=24)], env=(2, 150, 6), mods=((3, 1, 0.5),))
fadd("Bass | Tight Low-Cut", [fs_("hp", 40, 10, slope=48)])
fadd("Vocal | Radio Voice", [fs_("hp", 500, 25, slope=24), fs_("lp", 3500, 30, slope=24), fs_("peak", 1800, 30, gain=5)])
fadd("Vocal | Formant Talk (vowel)", [fs_("vowel", 400, 50)], lfo1=(1, 0.5, 0), mods=((1, 1, 0.35),))
fadd("Synth | Dual Peak Character", [fs_("peak", 300, 30, gain=6), fs_("peak", 4000, 40, gain=5), fs_("lp", 9000, 10)])
fadd("Synth | Comb Metallic", [fs_("comb", 220, 80, mix=70)])
fadd("Pad | Stereo Drift", [fs_("lp", 1500, 40, pan=-60), fs_("lp", 1500, 40, pan=60)], routing="parallel", lfo1=(0, 0.13, 0), lfo2=(0, 0.17, 0), mods=((1, 1, 0.3), (2, 2, -0.3)))
fadd("Mix | Gentle Tilt Dark", [fs_("lp", 9000, 5, slope=12), fs_("peak", 200, 10, gain=2)])
fadd("Mix | Notch Out a Whistle", [fs_("notch", 3150, 80, slope=12)])
fadd("FX | Underwater", [fs_("lp", 400, 60, slope=48)], lfo1=(0, 0.3, 0), mods=((1, 1, 0.15),))
fadd("FX | Rising Build (sweep up)", [fs_("hp", 60, 40, slope=24)], lfo1=(2, 0.05, 2), mods=((1, 1, 1.0),))
# Advanced
fadd("Routing | Two Pairs: Lo-Fi + Air", [fs_("hp", 300), fs_("lp", 2500), fs_("hp", 8000, slope=12), fs_("peak", 12000, 20, gain=4)], adv=True, routing="pairs")
fadd("Routing | Mid Warm, Side Bright", [fs_("lp", 6000, 10), OFF, fs_("hp", 300, 10), fs_("peak", 9000, 20, gain=4)], adv=True, routing="ms")
fadd("Routing | Parallel Band Splitter", [fs_("lp", 250, 30), fs_("bp", 1000, 50, slope=12), fs_("bp", 3000, 50, slope=12), fs_("hp", 8000, 30)], adv=True, routing="parallel")
fadd("Mod | Two LFOs Against Each Other", [fs_("lp", 1200, 55), fs_("hp", 300, 40)], adv=True, lfo1=(0, 0.5, 0), lfo2=(1, 0.33, 0), mods=((1, 1, 0.4), (2, 2, 0.35), (1, 5, -0.3)))
fadd("Mod | Envelope Ducks the Resonance", [fs_("lp", 900, 80)], adv=True, env=(1, 200, 0), mods=((3, 5, -0.8),))
fadd("Mod | Comb Sweep Flange", [fs_("comb", 400, 85, mix=50)], adv=True, lfo1=(1, 0.15, 0), mods=((1, 1, 0.5),))
fadd("Self-Oscillation | Whistling LP", [fs_("lp", 1000, 98, slope=24)], adv=True, lfo1=(0, 0.2, 0), mods=((1, 1, 0.5),), out=-12)
fadd("Drive | Screaming 48 dB", [fs_("lp", 700, 85, drive=24, slope=48)], adv=True, env=(2, 100, 6), mods=((3, 1, 0.5),), out=-6)

def fissure_cpp():
    rows = []
    for name, adv, routing, sl, lfo1, lfo2, env, mods, mix, out in FI:
        vals = [("lfo1Rate", lfo1[1]), ("lfo2Rate", lfo2[1]), ("envAttack", env[0]), ("envRelease", env[1]), ("envGain", env[2]), ("mix", mix), ("output", out)]
        props = [("routing", routing), ("lfo1Shape", lfo1[0]), ("lfo1Sync", lfo1[2]), ("lfo2Shape", lfo2[0]), ("lfo2Sync", lfo2[2])]
        for i, s in enumerate(sl):
            n = i + 1
            vals += [(f"f{n}cut", s["cut"]), (f"f{n}res", s["res"]), (f"f{n}drive", s["drive"]), (f"f{n}gain", s["gain"]), (f"f{n}pan", s["pan"]), (f"f{n}mix", s["mix"])]
            props += [(f"on{n}", s["on"]), (f"type{n}", s["t"]), (f"slope{n}", s["slope"])]
        for i, (src, dst, amt) in enumerate(mods):
            vals.append((f"mod{i+1}", amt)); props += [(f"src{i+1}", src), (f"dst{i+1}", dst)]
        vs = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in vals)
        ps = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props)
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {vs} }},\n            {{ {ps} }})')
    return "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_fissure()   // Fissure Filter\n{\n    return {\n" + ",\n".join(rows) + "\n    };\n}\n"

def cpp():
    out = ["// Generated by scripts/gen_presets_114.py - factory presets for the 1.14 Studio Line. Do not edit by hand.",
           "// Included in namespace SignaturePresets (SignaturePresets.h); bank() calls add114().", "",
           "SQ_PRESETS_NOINLINE inline std::vector<Preset> presets114_spectra()   // Spectra EQ", "{", "    return {"]
    rows = []
    for name, adv, phase, bands, outDb, auto in P:
        vals, props = [], [("phaseMode", phase), ("autoGain", 1 if auto else 0), ("linQuality", 2)]
        for i, (shape, freq, gain, q, slope, place, dyn) in enumerate(bands):
            n = i + 1
            vals += [(f"b{n}Freq", freq), (f"b{n}Gain", gain), (f"b{n}Q", q)]
            props += [(f"b{n}On", 1), (f"b{n}Shape", SHAPES[shape]), (f"b{n}Place", PLACES[place]), (f"b{n}Slope", slope), (f"b{n}Dyn", 1 if dyn else 0)]
            if dyn:
                th, rg, at, rl = dyn
                vals += [(f"b{n}Thresh", th), (f"b{n}Range", rg)]
                props += [(f"b{n}Att", at), (f"b{n}Rel", rl)]
        if outDb: vals.append(("output", outDb))
        v = ", ".join(f'N ("{k}", {float(x)}f)' for k, x in vals)
        pr = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in props)
        rows.append(f'        {"A" if adv else "B"} ("{name}", {{ {v} }},\n            {{ {pr} }})')
    out.append(",\n".join(rows))
    out += ["    };", "}", "", torque_cpp(), summit_cpp(), atrium_cpp(), mb2_cpp(), sibyl_cpp(), portcullis_cpp(), kiln_cpp(), spindle_cpp(), fissure_cpp(), "inline void add114 (std::map<juce::String, std::vector<Preset>>& m)", "{", '    m["Spectra EQ"] = presets114_spectra();', '    m["Torque Comp"] = presets114_torque();', '    m["Summit Limiter"] = presets114_summit();', '    m["Atrium Verb"] = presets114_atrium();', '    m["Apex Multiband 2"] = presets114_mb2();', '    m["Sibyl De-Ess"] = presets114_sibyl();', '    m["Portcullis Gate"] = presets114_portcullis();', '    m["Kiln"] = presets114_kiln();', '    m["Spindle Delay"] = presets114_spindle();', '    m["Fissure Filter"] = presets114_fissure();', "}", ""]
    return "\n".join(out)

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "Source", "SignaturePresets114.inl"), "w").write(cpp())
print(len(P), "Spectra EQ presets,", sum(1 for p in P if not p[1]), "basic;", len(T), "Torque Comp presets,", sum(1 for t in T if not t[1]), "basic;", len(S), "Summit presets,", len(AV), "Atrium presets,", len(MB), "MB2 presets,", len(SB), "Sibyl presets,", len(PG), "Portcullis presets,", len(KL), "Kiln presets,", len(SD), "Spindle presets,", len(FI), "Fissure presets")
