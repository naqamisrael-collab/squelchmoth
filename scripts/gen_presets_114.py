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
    out += ["    };", "}", "", torque_cpp(), summit_cpp(), atrium_cpp(), "inline void add114 (std::map<juce::String, std::vector<Preset>>& m)", "{", '    m["Spectra EQ"] = presets114_spectra();', '    m["Torque Comp"] = presets114_torque();', '    m["Summit Limiter"] = presets114_summit();', '    m["Atrium Verb"] = presets114_atrium();', "}", ""]
    return "\n".join(out)

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "Source", "SignaturePresets114.inl"), "w").write(cpp())
print(len(P), "Spectra EQ presets,", sum(1 for p in P if not p[1]), "basic;", len(T), "Torque Comp presets,", sum(1 for t in T if not t[1]), "basic;", len(S), "Summit presets,", len(AV), "Atrium presets")
