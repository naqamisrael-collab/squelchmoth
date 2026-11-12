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
    out += ["    };", "}", "", "inline void add114 (std::map<juce::String, std::vector<Preset>>& m)", "{", '    m["Spectra EQ"] = presets114_spectra();', "}", ""]
    return "\n".join(out)

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "Source", "SignaturePresets114.inl"), "w").write(cpp())
print(len(P), "Spectra EQ presets,", sum(1 for p in P if not p[1]), "basic")
