#!/usr/bin/env python3
"""Factory presets for the 1.11 Color & Space / Voice plug-ins.

Every preset is written for a stated use; its name says what it does. Output trims are not guessed: the presets are
rendered through the real DSP on pink noise, a drum loop and a sung vocal (tests/preset_calib.cpp) and the trim that
centres the three level changes is written back, so each preset holds the input level unless loudness change is its
purpose (Lo-Fi / Destroy and the like).

    scripts/gen_presets_111.py            # writes tests/presets111_table.h, builds + runs the calibrator,
                                          # then writes Source/SignaturePresets111.inl
"""
import json, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ----------------------------------------------------------------------------------------------- Cinder
# model ids: 0 Tape Preamp, 1 British Channel, 2 Germanium Class-A, 3 Triode Box, 4 Pentode Box,
#            5 Transformer Iron, 6 Op-Amp Rail, 7 Diode Fuzz, 8 Tape Hysteresis, 9 Broadcast Limiter Amp
TP, BC, GE, TR, PE, IR, OA, DF, TH, BL = range(10)
ST, MS, MID, SIDE = range(4)

def C(name, model, drive, adv=False, **k):
    d = dict(name=name, model=model, modelB=k.get('modelB', TP), blend=k.get('blend', 0), drive=drive,
             bias=k.get('bias', 0), dynamics=k.get('dyn', 30), lowCut=k.get('lc', 20), thump=k.get('thump', 0),
             tone=k.get('tone', 0), highCut=k.get('hc', 22000), steep=k.get('steep', False), mix=k.get('mix', 100),
             punish=k.get('punish', False), ms=k.get('ms', ST), oversample=k.get('os', 2), linear=k.get('linear', False),
             autoGain=k.get('auto', True), adv=adv or k.get('blend', 0) > 0 or k.get('ms', ST) != ST or k.get('os', 2) != 2 or k.get('linear', False))
    return d

CINDER = [
    # Vocal
    C("Vocal | Warm Tape Lift", TP, 4, bias=10, lc=80, tone=1, hc=18000),
    C("Vocal | Console Presence", BC, 6, lc=90, tone=1.5),
    C("Vocal | Airy Glow", TP, 3, modelB=TR, blend=30, tone=2.5, hc=20000),
    C("Vocal | Gritty Rap Edge", PE, 10, dyn=40, lc=120, tone=1, mix=70),
    C("Vocal | Breathy Pop Sheen", TH, 2, lc=100, tone=3, hc=20000),
    C("Vocal | Thick Ballad Body", IR, 5, lc=100, thump=2, tone=-1),
    C("Vocal | Parallel Diode Bite", DF, 14, lc=120, mix=25),
    C("Vocal | Germanium Focus", GE, 4, lc=90),
    C("Vocal | Soft Tube Polish", TR, 2, dyn=20),
    C("Vocal | Rock Scream Edge", PE, 14, modelB=DF, blend=40, lc=120, hc=14000),
    C("Vocal | Vintage Tape Double", TH, 8, hc=14000, tone=-1),
    C("Vocal | Whisper Warmth", TP, 0, dyn=0, tone=2),
    C("Vocal | Mid Focus (M/S)", BC, 6, ms=MID, lc=90),
    C("Vocal | Broadcast Announcer", BL, 6, lc=110, tone=1.5),
    # Drums
    C("Drums | Kick Punch", TR, 8, lc=30, thump=3, dyn=50),
    C("Drums | Kick Beater Click", PE, 10, lc=40, tone=3),
    C("Drums | Kick Tape Round", TH, 10, hc=12000),
    C("Drums | Snare Crack", OA, 8, lc=120, tone=2, dyn=60),
    C("Drums | Snare Fat Body", IR, 8, lc=80, thump=2),
    C("Drums | Snare Grit", DF, 10, mix=40),
    C("Drums | Toms Warm Sustain", TP, 6, dyn=20),
    C("Drums | Hats Soft Edge", TH, 4, lc=150, hc=16000),
    C("Drums | Overheads Glue", BC, 3, dyn=20, tone=1),
    C("Drums | Clap Bite", PE, 9, tone=2),
    C("Drums | Rimshot Snap", OA, 6, dyn=70),
    C("Drums | Percussion Air", TP, 4, tone=2),
    C("Drums | Room Mic Squash", BL, 14, modelB=DF, blend=30, mix=60),
    # Drum Bus
    C("Drum Bus | Glue and Weight", BC, 5, dyn=30),
    C("Drum Bus | Tape Bus Warmth", TH, 6),
    C("Drum Bus | Crushed Room Glue", BL, 12, mix=60),
    C("Drum Bus | Punchy Transients", TR, 6, dyn=80),
    C("Drum Bus | Gentle Iron", IR, 4),
    C("Drum Bus | Boom Bap Grit", GE, 10, lc=40, hc=12000),
    C("Drum Bus | Trap Clean Loud", OA, 6, dyn=10),
    C("Drum Bus | Breakbeat Crunch", PE, 12, lc=60, hc=10000, mix=70),
    C("Drum Bus | Rock Kit Density", TR, 7, modelB=BC, blend=50),
    C("Drum Bus | House Kick Glue", TP, 5, lc=30, thump=2),
    C("Drum Bus | Side Grit Width (M/S)", BC, 8, ms=SIDE),
    C("Drum Bus | Germanium Snap", GE, 6, dyn=60),
    C("Drum Bus | Tube Bus 8x", TR, 5, os=3),
    # Bass
    C("Bass | DI Warmth", TP, 6),
    C("Bass | Growl Edge", PE, 12, lc=30, mix=60),
    C("Bass | Fuzz Bass", DF, 18, mix=50),
    C("Bass | Motown-Era Thump", IR, 6, hc=6000, thump=1, lc=35),
    C("Bass | Synth Bass Bite", OA, 10, tone=2),
    C("Bass | Upright Body", TH, 4, lc=40, tone=-1),
    C("Bass | Pick Bass Clank", PE, 8, tone=3),
    C("Bass | Germanium Weight", GE, 8),
    C("Bass | Console Lows", BC, 7, lc=35, thump=2),
    C("Bass | Parallel Grind", DF, 20, mix=30),
    C("Bass | Tube Round", TR, 6, dyn=20),
    C("Bass | Dub Warmth", TP, 8, hc=5000),
    C("Bass | Slap Crunch", OA, 8, modelB=PE, blend=50, dyn=60),
    # 808
    C("808 | Sub Harmonics", TR, 10, bias=40),
    C("808 | Distorted Trap", DF, 16),
    C("808 | Tape Saturated Sub", TH, 12),
    C("808 | Hard Clip Loud", OA, 14, dyn=0),
    C("808 | Iron Weight", IR, 10),
    C("808 | Germanium Growl", GE, 12),
    C("808 | Phone Speaker Translate", PE, 8, mix=50, bias=30),
    C("808 | Drill Grit", DF, 12, modelB=OA, blend=50),
    C("808 | Warm Round", TP, 8),
    C("808 | Mono Mid Drive (M/S)", BC, 8, ms=MID),
    C("808 | Punch Thump", TR, 8, lc=30, thump=3, dyn=50),
    C("808 | Parallel Fuzz", DF, 20, mix=30),
    # Guitar
    C("Guitar | Clean Tube Sparkle", TR, 2, tone=1),
    C("Guitar | Edge of Breakup", TR, 10, dyn=50),
    C("Guitar | Pentode Crunch", PE, 14, lc=80, hc=10000),
    C("Guitar | Fuzz Lead", DF, 22, hc=9000),
    C("Guitar | Vintage Germanium Fuzz", GE, 20, hc=8000),
    C("Guitar | Acoustic Body", TP, 3, lc=80),
    C("Guitar | Acoustic Shimmer", BC, 2, tone=2),
    C("Guitar | Amp-less Direct Grit", PE, 10, modelB=IR, blend=40, hc=9000),
    C("Guitar | Slide Blues", TR, 12, hc=8000),
    C("Guitar | Funk Clean Snap", OA, 4, dyn=60),
    C("Guitar | Shoegaze Wall", DF, 24, modelB=TH, blend=40, hc=7000),
    C("Guitar | Tape Slap Warmth", TH, 6),
    C("Guitar | Doubled Sides (M/S)", BC, 6, ms=SIDE),
    # Keys
    C("Keys | Electric Piano Bark", TR, 10, dyn=60),
    C("Keys | Tine Piano Bell Grit", PE, 6, tone=1),
    C("Keys | Organ Overdrive", PE, 14),
    C("Keys | Piano Warmth", TP, 3),
    C("Keys | Upright Piano Tape", TH, 6, hc=12000),
    C("Keys | Clavinet-Style Funk Bite", OA, 8, tone=2),
    C("Keys | Reed Piano Growl", TR, 12, bias=30),
    C("Keys | Mellow Iron", IR, 6, tone=-2),
    C("Keys | Broadcast Piano", BL, 6),
    C("Keys | Germanium Keys", GE, 6),
    C("Keys | Pad Thickener", BC, 6, mix=60),
    C("Keys | Harpsichord Edge", PE, 4, tone=2),
    # Synth
    C("Synth | Lead Scream", DF, 18),
    C("Synth | Analog Warmth", TP, 6),
    C("Synth | Acid Squelch", PE, 16, dyn=70),
    C("Synth | Pad Glow", TR, 4, mix=70),
    C("Synth | Pluck Edge", OA, 6, dyn=60),
    C("Synth | Supersaw Glue", BC, 4),
    C("Synth | Reese Grit", GE, 12),
    C("Synth | Hard Rail Lead", OA, 16),
    C("Synth | Tape Arp", TH, 8),
    C("Synth | Bass Synth Weight", IR, 8),
    C("Synth | Retro Broadcast", BL, 8, hc=10000),
    C("Synth | Wide Side Grit (M/S)", PE, 10, ms=SIDE),
    C("Synth | Parallel Lead Fuzz", DF, 20, mix=35),
    # Mix Bus
    C("Mix Bus | Gentle Glue", BC, 2, dyn=20, os=3),
    C("Mix Bus | Tape Master Warmth", TH, 3, os=3),
    C("Mix Bus | Analog Sum Density", IR, 3, os=3),
    C("Mix Bus | Tube Sheen", TP, 2, tone=1, os=3),
    C("Mix Bus | Loud and Clean", OA, 4, dyn=0, os=3),
    C("Mix Bus | Vintage Broadcast", BL, 3, os=3),
    C("Mix Bus | Mid Weight, Sides Clean (M/S)", BC, 4, ms=MID, os=3),
    C("Mix Bus | Side Air (M/S)", TP, 3, ms=SIDE, tone=2, os=3),
    C("Mix Bus | Hip-Hop Weight", GE, 4, thump=1, lc=25, os=3),
    C("Mix Bus | Rock Push", TR, 4, modelB=BC, blend=50, os=3),
    C("Mix Bus | Linear-Phase Iron", IR, 2, linear=True, os=3),
    C("Mix Bus | Master Tape 16x", TH, 4, os=4),
    # Lo-Fi / Destroy (loudness and tone change are the point: no +-1 dB rule)
    C("Lo-Fi / Destroy | Telephone", DF, 10, lc=400, hc=3400, steep=True),
    C("Lo-Fi / Destroy | AM Radio", BL, 10, lc=300, hc=4500, steep=True),
    C("Lo-Fi / Destroy | Megaphone", PE, 20, lc=600, hc=5000, steep=True),
    C("Lo-Fi / Destroy | Blown Speaker", DF, 16, punish=True),
    C("Lo-Fi / Destroy | Total Annihilation", DF, 16, modelB=OA, blend=50, punish=True),
    C("Lo-Fi / Destroy | Cassette Mush", TH, 14, hc=7000),
    C("Lo-Fi / Destroy | Lo-Fi Drum Bus", GE, 18, hc=6000),
    C("Lo-Fi / Destroy | Lo-Fi Keys", TH, 12, hc=5000, lc=120),
    C("Lo-Fi / Destroy | Broken Console", BC, 10, punish=True),
    C("Lo-Fi / Destroy | Walkie-Talkie", OA, 24, lc=800, hc=3000, steep=True),
    C("Lo-Fi / Destroy | Old Record Haze", TP, 6, hc=6000, lc=150),
    C("Lo-Fi / Destroy | Clip to Oblivion", OA, 12, punish=True, dyn=0),
    C("Lo-Fi / Destroy | Starved Fuzz", GE, 26, bias=-60),
    # Parallel
    C("Parallel | Drum Crush 30%", BL, 20, mix=30),
    C("Parallel | Vocal Excite", PE, 12, mix=20, lc=200, tone=3),
    C("Parallel | Bass Harmonics", DF, 16, mix=30, lc=150),
    C("Parallel | 808 Ear Candy", TR, 20, mix=25, lc=120),
    C("Parallel | Snare Sizzle", OA, 18, mix=25, tone=4),
    C("Parallel | Mix Bus Thickener", TH, 10, mix=20),
    C("Parallel | Guitar Grit Blend", PE, 18, mix=35),
    C("Parallel | Synth Fuzz Layer", DF, 20, mix=30),
    C("Parallel | Room Squash 40%", BL, 24, mix=40),
    C("Parallel | Keys Body", IR, 14, mix=30),
    C("Parallel | Germanium Glow 25%", GE, 14, mix=25),
    C("Parallel | Side-Only Grit (M/S)", DF, 18, ms=SIDE, mix=40),
]

# ----------------------------------------------------------------------------------------------- Wingbeat Echo
# styles: 0 ST30 1 ST15 2 ST7.5 3 Tube Tape 4 Spring Tape 5 Bucket Pedal 6 Bucket Dark 7 Oil Can 8 Drum Echo 9 Chorus Box
# 10 AM Radio 11 Telephone 12 Shortwave 13 Transistor Radio 14 Digital Clean 15 Digital 12-Bit 16 Rack Digital 1980
# 17 Distressed 18 Diffused 19 Ambient 20 Splatter 21 Warm Analog Pedal 22 Plate Head Echo 23 Ensemble Echo 24 Lo-Fi Sampler
# 25 Rockabilly Slap 26 Dub Siren Tape 27 Broken Tape 28 Pitch Drift 29 Small Studio Tape 30 Lunar Tape 31 Ice Diffuse
# 32 Granule 33 Dust Cassette 34 Reverse Bloom 35 Infinite Wash
# sync (WingMod divisions): 0 1/64 3 1/32 6 1/16 7 1/16. 8 1/16T 9 1/8 10 1/8. 11 1/8T 12 1/4 13 1/4. 14 1/4T 15 1/2 16 1/2. 18 1 bar 21 2 bars
SINGLE, DUAL, PING, RHYTHM = range(4)
def W(name, style, t, fb, mix, adv=False, **k):
    sync = t if isinstance(t, int) else -1
    ms = t if isinstance(t, float) else 375.0
    tr = k.get('tr', ms * 1.5 if isinstance(t, float) else 500.0)
    syncR = k.get('syncR', -1)
    d = dict(name=name, style=style, mode=k.get('mode', SINGLE), syncL=sync, syncR=syncR, time=ms, timeR=tr, feedback=fb, mix=mix,
             saturation=k.get('sat', 50), groove=k.get('groove', 0), feel=k.get('feel', 0), width=k.get('width', 100), offset=k.get('off', 0),
             accent=k.get('accent', 0), warp=k.get('warp', 0), lowCut=k.get('lc', 20), highCut=k.get('hc', 20000), wow=k.get('wow', 100),
             flutter=k.get('flutter', 100), diffusion=k.get('diff', 0), duck=k.get('duck', 0), duckRelease=k.get('dr', 250), heads=k.get('heads', 1),
             limiter=k.get('lim', 1), pattern=k.get('pat', 0), timeChange=k.get('tc', 0), prime=k.get('prime', False))
    d['adv'] = adv or d['mode'] in (DUAL, RHYTHM) or d['duck'] > 0 or d['heads'] != 1 or d['prime']
    return d

WINGBEAT = [
    # Vocal Slaps
    W("Vocal Slaps | Classic Tape Slap", 25, 95.0, 8, 22, lc=150, hc=9000),
    W("Vocal Slaps | Studio Tape Slap", 1, 110.0, 5, 20, lc=120),
    W("Vocal Slaps | Rap Double Slap", 29, 75.0, 0, 18, lc=180, hc=10000),
    W("Vocal Slaps | Tube Slap Warm", 3, 120.0, 10, 22, lc=140, hc=7000),
    W("Vocal Slaps | Short Stereo Slap", 0, 85.0, 5, 20, mode=DUAL, tr=110.0, width=130),
    W("Vocal Slaps | Digital Doubler Slap", 14, 60.0, 0, 18, lc=200, hc=12000),
    W("Vocal Slaps | Gritty Slap", 20, 100.0, 12, 18, sat=70, lc=200, hc=8000),
    W("Vocal Slaps | Pop Vocal Ambience", 18, 140.0, 15, 16, diff=30, lc=200, hc=9000),
    W("Vocal Slaps | Country Slap", 25, 130.0, 12, 22, lc=150),
    W("Vocal Slaps | Spring Slap", 4, 90.0, 10, 20, heads=1, lc=150),
    W("Vocal Slaps | Bucket Slap", 5, 80.0, 10, 20, lc=150),
    W("Vocal Slaps | Ducked Slap", 1, 100.0, 15, 25, duck=10, lc=150),
    W("Vocal Slaps | Wide Slap Pair", 14, 70.0, 0, 20, mode=DUAL, tr=95.0, width=150),
    W("Vocal Slaps | Lo-Fi Slap", 33, 110.0, 10, 20, lc=200, hc=6000),
    W("Vocal Slaps | Prime Slap (No Comb)", 14, 90.0, 10, 20, prime=True),
    W("Vocal Slaps | Telephone Slap", 11, 120.0, 10, 18),
    # Throws
    W("Throws | Quarter Throw", 1, 12, 40, 30, duck=12, lc=200, hc=8000),
    W("Throws | Dotted Eighth Throw", 3, 10, 45, 30, duck=12, lc=200, hc=7000),
    W("Throws | Half Note Hall Throw", 18, 15, 50, 28, duck=10, diff=40, lc=250),
    W("Throws | Ping-Pong Throw", 0, 12, 45, 30, mode=PING, duck=10, lc=200),
    W("Throws | Dark Tape Throw", 2, 12, 50, 30, duck=12, lc=200),
    W("Throws | Long Ambient Throw", 19, 15, 65, 28, duck=10, lc=250),
    W("Throws | Spring Tape Throw", 4, 13, 50, 28, heads=3, duck=10, lc=200),
    W("Throws | Eighth Triplet Throw", 1, 11, 40, 28, duck=10, lc=200),
    W("Throws | Bucket Throw", 5, 12, 50, 28, duck=10, lc=200),
    W("Throws | Big Ballad Throw", 22, 15, 55, 30, duck=12, lc=200, hc=7000),
    W("Throws | Reverse Bloom Throw", 34, 12, 30, 28, lc=200),
    W("Throws | Shimmer Diffuse Throw", 31, 15, 55, 26, duck=8),
    W("Throws | Rap Ad-Lib Throw", 29, 9, 35, 30, duck=12, lc=250, hc=9000),
    W("Throws | Telephone Throw", 11, 12, 40, 26),
    W("Throws | Lunar Throw", 30, 13, 55, 28, duck=10, lc=200),
    W("Throws | Oil Can Throw", 7, 12, 45, 28, duck=10, lc=200),
    # Dub
    W("Dub | Siren Feedback", 26, 10, 75, 35, lc=150),
    W("Dub | Skank Echo", 26, 9, 60, 35, lc=200, hc=3500),
    W("Dub | Dub Rhythm Pattern", 26, 12, 40, 35, mode=RHYTHM, pat=9),
    W("Dub | Melodica Tape", 3, 13, 65, 32, lc=150),
    W("Dub | Rim Shot Splash", 20, 10, 70, 30, lc=300),
    W("Dub | Sound System Space", 4, 12, 70, 35, heads=5),
    W("Dub | Steppers Tape", 1, 10, 60, 32, lc=150, hc=5000),
    W("Dub | Spring Heads 1+3", 4, 12, 55, 32, heads=5, lc=150),
    W("Dub | All Heads Wash", 4, 12, 50, 30, heads=7, lc=150),
    W("Dub | Dark Bucket Dub", 6, 10, 65, 32, lc=150),
    W("Dub | King Tubby-Style Filter Dub".replace("King Tubby-Style ", "Classic "), 26, 12, 70, 32, lc=400, hc=3000),
    W("Dub | Echo Chamber Dub", 22, 10, 60, 30, diff=20),
    W("Dub | Ping-Pong Dub", 26, 12, 65, 32, mode=PING),
    W("Dub | Swing Dub", 26, 10, 60, 32, mode=PING, groove=20),
    W("Dub | Warped Dub", 27, 12, 65, 30, warp=60),
    W("Dub | Infinite Dub Wash", 35, 12, 80, 30),
    # Tape
    W("Tape | 30 ips Clean Echo", 0, 12, 35, 25),
    W("Tape | 15 ips Warm Echo", 1, 12, 40, 25),
    W("Tape | 7.5 ips Dark Echo", 2, 12, 45, 25),
    W("Tape | Tube Tape Three Heads", 3, 12, 45, 25),
    W("Tape | Spring Tape Head 1", 4, 12, 40, 25, heads=1),
    W("Tape | Spring Tape Head 2", 4, 12, 40, 25, heads=2),
    W("Tape | Spring Tape Heads 2+3", 4, 12, 40, 25, heads=6),
    W("Tape | Plate Head Echo", 22, 12, 45, 25),
    W("Tape | Small Studio Deck", 29, 12, 35, 25),
    W("Tape | Distressed Tape", 17, 12, 45, 25),
    W("Tape | Broken Tape", 27, 12, 40, 25),
    W("Tape | Splatter Tape", 20, 12, 50, 22, sat=80),
    W("Tape | Wow-Heavy Tape", 2, 12, 45, 25, wow=200),
    W("Tape | Flutter Heavy", 1, 12, 40, 25, flutter=200),
    W("Tape | No Wobble Tape", 1, 12, 40, 25, wow=0, flutter=0),
    W("Tape | Glued Mix Tape", 0, 13, 25, 15),
    # Rhythmic
    W("Rhythmic | Dotted Eighth Guitar", 14, 10, 40, 30, lc=150),
    W("Rhythmic | Quarter + Dotted Pattern", 1, 12, 30, 30, mode=RHYTHM, pat=0),
    W("Rhythmic | Gallop", 14, 12, 25, 30, mode=RHYTHM, pat=1),
    W("Rhythmic | Tresillo 3-3-2", 21, 12, 30, 30, mode=RHYTHM, pat=2),
    W("Rhythmic | Clave 3-2", 14, 12, 25, 28, mode=RHYTHM, pat=3),
    W("Rhythmic | Triplet Cascade", 1, 12, 30, 30, mode=RHYTHM, pat=4),
    W("Rhythmic | Build-Up Roll", 14, 12, 20, 28, mode=RHYTHM, pat=5),
    W("Rhythmic | Swing Eighths", 1, 12, 30, 30, mode=RHYTHM, pat=10),
    W("Rhythmic | Sixteenth Stutter", 14, 6, 50, 30),
    W("Rhythmic | Eighth Ping-Pong", 14, 9, 45, 30, mode=PING),
    W("Rhythmic | Swung Ping-Pong", 1, 9, 45, 30, mode=PING, groove=25),
    W("Rhythmic | Dual 3:4 Polyrhythm", 14, 13, 40, 28, mode=DUAL, syncR=12),
    W("Rhythmic | Dual Eighth vs Dotted", 1, 9, 40, 28, mode=DUAL, syncR=10),
    W("Rhythmic | Pushed Feel", 14, 12, 35, 28, feel=-6),
    W("Rhythmic | Dragged Feel", 14, 12, 35, 28, feel=6),
    W("Rhythmic | Accent Ping-Pong", 14, 9, 45, 30, mode=PING, accent=60),
    # Wide / Stereo
    W("Wide / Stereo | Wide Pairs Pattern", 14, 12, 20, 28, mode=RHYTHM, pat=8),
    W("Wide / Stereo | Haas Widener", 14, 1.0, 0, 30, mode=DUAL, tr=14.0, width=150),
    W("Wide / Stereo | Wide Tape Dual", 1, 12, 35, 28, mode=DUAL, syncR=13, width=140),
    W("Wide / Stereo | Ping-Pong Eighths Wide", 0, 9, 45, 28, mode=PING, width=150),
    W("Wide / Stereo | L/R Offset 12 ms", 14, 12, 35, 28, off=12, width=130),
    W("Wide / Stereo | Ensemble Wide", 23, 12, 35, 28, width=150),
    W("Wide / Stereo | Lunar Wide", 30, 13, 45, 28, width=150),
    W("Wide / Stereo | Wide Diffuse", 18, 12, 45, 26, width=150, diff=40),
    W("Wide / Stereo | Polyrhythm Wide", 1, 14, 40, 28, mode=DUAL, syncR=12, width=150),
    W("Wide / Stereo | Narrow Mono Echo", 1, 12, 40, 28, width=0),
    W("Wide / Stereo | Wide Slap", 25, 95.0, 5, 22, mode=DUAL, tr=125.0, width=150),
    W("Wide / Stereo | Stereo Bucket", 5, 12, 40, 28, mode=DUAL, syncR=10, width=130),
    W("Wide / Stereo | Wide Granules", 32, 12, 40, 28, width=150),
    W("Wide / Stereo | Wide Oil Can", 7, 12, 45, 28, width=150),
    W("Wide / Stereo | Cross Ping-Pong Dark", 2, 10, 55, 28, mode=PING, width=140),
    W("Wide / Stereo | 32-Tap Cloud", 14, 12, 10, 25, mode=RHYTHM, pat=11),
    # Chorus / Doubling
    W("Chorus / Doubling | Chorus Box Classic", 9, 7.0, 0, 50, wow=100),
    W("Chorus / Doubling | Slow Chorus", 9, 12.0, 0, 45, wow=150),
    W("Chorus / Doubling | Ensemble Chorus", 23, 10.0, 10, 45),
    W("Chorus / Doubling | Vocal Doubler", 14, 22.0, 0, 35, mode=DUAL, tr=31.0, wow=0, width=130),
    W("Chorus / Doubling | ADT-Style Double", 1, 28.0, 0, 35, wow=120),
    W("Chorus / Doubling | Flange Echo", 9, 3.0, 40, 40, wow=80),
    W("Chorus / Doubling | Bucket Chorus", 5, 9.0, 10, 45, wow=200),
    W("Chorus / Doubling | Tape Doubler", 0, 35.0, 0, 35, wow=160),
    W("Chorus / Doubling | Guitar Doubler Wide", 14, 18.0, 0, 40, mode=DUAL, tr=27.0, width=150, wow=0),
    W("Chorus / Doubling | Lunar Chorus", 30, 15.0, 10, 40),
    W("Chorus / Doubling | Pitch Drift Double", 28, 25.0, 0, 35),
    W("Chorus / Doubling | Oil Can Chorus", 7, 20.0, 20, 40),
    W("Chorus / Doubling | Rack Digital Chorus", 16, 12.0, 10, 40),
    W("Chorus / Doubling | Synth Pad Chorus", 23, 14.0, 15, 45),
    W("Chorus / Doubling | Thick Triple", 14, 18.0, 0, 40, mode=DUAL, tr=26.0, wow=100, width=150),
    W("Chorus / Doubling | Vibrato Echo", 9, 6.0, 0, 100, wow=120),
    # Lo-Fi / Radio
    W("Lo-Fi / Radio | AM Radio Echo", 10, 12, 35, 30),
    W("Lo-Fi / Radio | Telephone Line", 11, 9, 30, 35),
    W("Lo-Fi / Radio | Shortwave Drift", 12, 12, 45, 30),
    W("Lo-Fi / Radio | Transistor Radio", 13, 10, 35, 30),
    W("Lo-Fi / Radio | Dust Cassette", 33, 12, 40, 30),
    W("Lo-Fi / Radio | Lo-Fi Sampler Echo", 24, 9, 40, 30),
    W("Lo-Fi / Radio | 12-Bit Echo", 15, 12, 40, 28),
    W("Lo-Fi / Radio | Dark Bucket Lo-Fi", 6, 12, 45, 30),
    W("Lo-Fi / Radio | Broken Tape Lo-Fi", 27, 12, 45, 30),
    W("Lo-Fi / Radio | Distressed Radio", 17, 10, 40, 30),
    W("Lo-Fi / Radio | Lo-Fi Hip-Hop Keys", 33, 13, 35, 28),
    W("Lo-Fi / Radio | Phone Ping-Pong", 11, 9, 40, 30, mode=PING),
    W("Lo-Fi / Radio | Radio Throw", 10, 12, 50, 30, duck=10),
    W("Lo-Fi / Radio | Crushed Sampler Rhythm", 24, 12, 25, 30, mode=RHYTHM, pat=2),
    W("Lo-Fi / Radio | Cassette Wobble Max", 33, 12, 40, 28, wow=200, flutter=200),
    W("Lo-Fi / Radio | Walkie Echo", 11, 6, 50, 30),
    # Ambient
    W("Ambient | Infinite Wash", 35, 15, 85, 35),
    W("Ambient | Ice Diffuse", 31, 15, 70, 32),
    W("Ambient | Ambient Cloud", 19, 15, 70, 32),
    W("Ambient | Lunar Drift", 30, 18, 70, 32),
    W("Ambient | Granular Haze", 32, 15, 60, 32),
    W("Ambient | Reverse Swells", 34, 15, 45, 32),
    W("Ambient | Diffused Pad", 18, 15, 70, 32, diff=40),
    W("Ambient | Long Dark Tape", 2, 21, 65, 30),
    W("Ambient | Shimmer Fifths", 31, 12, 35, 30, mode=RHYTHM, pat=7),
    W("Ambient | Octave Ladder", 14, 12, 30, 30, mode=RHYTHM, pat=6),
    W("Ambient | Slow Ping-Pong Space", 19, 15, 70, 30, mode=PING),
    W("Ambient | Pitch Drift Clouds", 28, 15, 70, 30, diff=30),
    W("Ambient | Endless Bucket", 6, 15, 80, 30),
    W("Ambient | Wide Ambient Dual", 19, 15, 65, 30, mode=DUAL, syncR=16),
    W("Ambient | Ducked Ambient Pad", 35, 15, 75, 35, duck=12),
    W("Ambient | Glacier", 31, 18, 80, 30, diff=40),
    # Sound Design
    W("Sound Design | Self-Oscillating Tape", 20, 6, 105, 30, lim=1),
    W("Sound Design | Warp Monster", 27, 9, 70, 30, warp=100),
    W("Sound Design | Grain Storm", 32, 6, 70, 32),
    W("Sound Design | Reverse Pattern", 34, 12, 40, 30),
    W("Sound Design | Metallic Comb", 14, 3.0, 85, 35),
    W("Sound Design | Short Prime Comb", 14, 7.0, 80, 35, prime=True),
    W("Sound Design | Robot Telephone", 11, 1.5, 85, 35),
    W("Sound Design | Shortwave Sweep", 12, 3.0, 80, 30),
    W("Sound Design | Spring Machine Gun", 4, 3, 70, 30, heads=7),
    W("Sound Design | Octave Up Ladder", 14, 12, 30, 30, mode=RHYTHM, pat=6),
    W("Sound Design | Hard-Limited Wall", 20, 9, 100, 28, lim=2),
    W("Sound Design | Tape Stop Smear", 2, 15, 60, 30, tc=0, wow=200),
    W("Sound Design | Bucket Siren", 6, 3, 95, 30),
    W("Sound Design | 32-Tap Swarm", 14, 12, 40, 30, mode=RHYTHM, pat=11),
    W("Sound Design | Ducked Infinite", 35, 12, 95, 35, duck=18),
    W("Sound Design | Feedback Ping-Pong Riser", 1, 6, 90, 30, mode=PING),
]

JR = []
def J(name, st, t, fb, mix, **k):
    JR.append(dict(name=name, style=st, mode=k.get('mode', 0), sync=t if isinstance(t, int) else -1, time=t if isinstance(t, float) else 375.0,
                   feedback=fb, mix=mix, saturation=k.get('sat', 50), tone=k.get('tone', 20000), change=k.get('tc', 0)))
# Jr styles: 0 Studio Tape 15, 1 Tube Tape Echo, 2 Spring Tape Echo, 3 Bucket Pedal, 4 Oil Can, 5 Digital Clean, 6 AM Radio, 7 Lunar Tape
for cat, items in [
    ("Vocal", [("Tape Slap", 0, 100.0, 5, 20), ("Tube Slap", 1, 120.0, 10, 20), ("Quarter Throw", 0, 12, 40, 25), ("Dotted Throw", 1, 10, 45, 25),
               ("Digital Double", 5, 60.0, 0, 18), ("Lunar Throw", 7, 13, 50, 25), ("Bucket Slap", 3, 85.0, 10, 20), ("Wide Slap", 5, 90.0, 5, 20, 1), ("Radio Slap", 6, 110.0, 10, 18), ("Ping-Pong Throw", 0, 12, 45, 25, 2)]),
    ("Guitar", [("Dotted Eighth", 5, 10, 40, 30), ("Rockabilly", 0, 110.0, 10, 25), ("Spring Tape Echo", 2, 12, 45, 28), ("Bucket Lead", 3, 12, 45, 28), ("Oil Can Swirl", 4, 12, 45, 28),
                ("Wide Clean", 5, 9, 35, 25, 1), ("Ping-Pong Eighths", 5, 9, 40, 28, 2), ("Tube Tape Lead", 1, 13, 45, 28), ("Lunar Pad", 7, 15, 60, 30), ("Radio Lead", 6, 12, 40, 28)]),
    ("Keys", [("Tape Quarter", 0, 12, 40, 25), ("Wide Eighths", 5, 9, 35, 25, 1), ("Dark Bucket", 3, 12, 50, 28), ("Oil Can Keys", 4, 12, 45, 25), ("Lunar Keys", 7, 13, 55, 28),
              ("Spring Keys", 2, 12, 45, 25), ("Tube Keys", 1, 12, 40, 25), ("Ping-Pong Keys", 0, 9, 45, 25, 2), ("Radio Keys", 6, 12, 40, 25), ("Clean Triplet", 5, 11, 35, 25)]),
    ("Drums", [("Snare Slap", 0, 90.0, 10, 18), ("Clap Quarter", 5, 12, 35, 20), ("Dub Rim", 2, 10, 65, 25), ("Hat Ping-Pong", 5, 6, 40, 20, 2), ("Bucket Snare", 3, 9, 45, 22),
               ("Tape Kick Slap", 0, 70.0, 0, 15), ("Spring Snare", 2, 12, 50, 22), ("Wide Perc", 5, 9, 35, 22, 1), ("Lunar Perc", 7, 10, 45, 22), ("Radio Snare", 6, 9, 40, 20)]),
    ("Synth", [("Synth Eighths", 5, 9, 45, 28), ("Lead Dotted", 0, 10, 45, 28), ("Pad Lunar", 7, 15, 65, 30), ("Arp Ping-Pong", 5, 6, 40, 28, 2), ("Bass Slap", 0, 70.0, 0, 15),
               ("Oil Can Lead", 4, 12, 50, 28), ("Bucket Pad", 3, 13, 55, 28), ("Tube Lead", 1, 10, 45, 28), ("Wide Synth", 5, 12, 40, 28, 1), ("Spring Lead", 2, 12, 45, 28)]),
    ("Creative", [("Self-Oscillating", 0, 6, 98, 30), ("Glide Sweeps", 7, 12, 70, 30), ("Stretch Changes", 5, 12, 60, 30), ("Radio Washout", 6, 15, 80, 30), ("Spring Wash", 2, 12, 85, 30),
                  ("Bucket Siren", 3, 3, 90, 30), ("Oil Can Haze", 4, 15, 75, 30), ("Lunar Infinity", 7, 18, 90, 30), ("Comb Metal", 5, 3.0, 85, 30), ("Wide Feedback Ping", 0, 9, 85, 30, 2)]),
]:
    for it in items:
        nm, st, t, fb, mix = it[:5]; mode = it[5] if len(it) > 5 else 0
        J(cat + " | " + nm, st, t, fb, mix, mode=mode, tc=1 if nm == "Stretch Changes" else 0)

# ----------------------------------------------------------------------------------------------- Foil Plate
# plates: 0 Steel Classic 1 Gold Foil 2 Studio Steel 3 Light Plate 4 Solid-State Plate 5 Silver Mist 6 Iron Mammoth
# inputs: 0 Tube 1 Solid State 2 Clean 3 Tape
STEEL, GOLD, STUDIO, LIGHT, SOLID, SILVER, IRON = range(7)
TUBE, SS, CLEAN, TAPEIN = range(4)
def F(name, plate, decay, mix, adv=False, **k):
    d = dict(name=name, plate=plate, decay=decay, mix=mix, preDelay=k.get('pd', 10), lowCut=k.get('lc', 80), highCut=k.get('hc', 12000), mod=k.get('mod', 30),
             modRate=k.get('mr', 0.8), width=k.get('width', 100), balance=k.get('bal', 0), eqLow=k.get('el', 0), eqHigh=k.get('eh', 0), autoDecay=k.get('ad', 0),
             autoThreshold=k.get('at', -30), drive=k.get('drv', 50), input=k.get('inp', TUBE), lowSlope=k.get('ls', 1), highSlope=k.get('hs', 1),
             freeze=False, darkening=k.get('dark', False), monoIn=k.get('mono', False))
    d['adv'] = adv or d['autoDecay'] != 0 or d['monoIn'] or d['balance'] != 0 or d['lowSlope'] != 1 or d['highSlope'] != 1
    return d

FOIL = [
    # Vocal Plates
    F("Vocal Plates | Classic Lead Vocal Plate", STEEL, 1.8, 22, pd=25, lc=150),
    F("Vocal Plates | Gold Foil Air", GOLD, 1.6, 20, pd=20, lc=180, eh=2),
    F("Vocal Plates | Ballad Long Plate", STEEL, 3.2, 24, pd=40, lc=150),
    F("Vocal Plates | Rap Vocal Short Plate", LIGHT, 0.9, 16, pd=15, lc=200, hc=10000),
    F("Vocal Plates | Pop Sheen Plate", SILVER, 1.7, 20, pd=30, lc=180),
    F("Vocal Plates | Studio Steel Warm Vocal", STUDIO, 2.0, 20, pd=25, lc=150, hc=9000),
    F("Vocal Plates | Ducked Vocal Plate (blooms in the gaps)", STEEL, 2.4, 25, pd=30, lc=150, ad=-70, at=-32),
    F("Vocal Plates | Backing Vocal Wash", GOLD, 2.6, 30, pd=20, lc=200, width=130),
    F("Vocal Plates | Soul Vocal Tube Plate", STEEL, 2.2, 22, pd=35, lc=140, drv=65),
    F("Vocal Plates | Female Vocal Shimmer", SILVER, 2.0, 22, pd=35, lc=200, eh=2),
    F("Vocal Plates | Male Vocal Body", STUDIO, 1.9, 20, pd=25, lc=120),
    F("Vocal Plates | Doo-Wop Plate", STEEL, 2.5, 28, pd=10, lc=150, inp=TAPEIN),
    F("Vocal Plates | Modern R&B Plate", LIGHT, 1.5, 20, pd=45, lc=220, eh=1),
    F("Vocal Plates | Spoken Word Plate", LIGHT, 0.7, 12, pd=10, lc=200, hc=8000),
    F("Vocal Plates | Choir Plate", IRON, 3.0, 25, pd=30, lc=150),
    F("Vocal Plates | Lengthen While Singing", GOLD, 1.5, 22, pd=30, lc=180, ad=60, at=-30),
    F("Vocal Plates | Solid-State Vocal Plate", SOLID, 1.8, 20, pd=20, lc=180, inp=SS),
    # Snare & Drums
    F("Snare & Drums | Classic Snare Plate", STEEL, 1.6, 25, pd=5, lc=200),
    F("Snare & Drums | 80s Snare Plate", SOLID, 2.2, 30, pd=0, lc=200, inp=SS, eh=2),
    F("Snare & Drums | Tight Snare Plate", LIGHT, 0.8, 22, pd=5, lc=250),
    F("Snare & Drums | Gated-Style Snare (auto-decay)", SOLID, 2.0, 28, pd=0, lc=200, ad=-90, at=-24),
    F("Snare & Drums | Fat Snare Iron", IRON, 1.4, 22, pd=5, lc=150),
    F("Snare & Drums | Rimshot Shimmer", GOLD, 1.2, 22, pd=5, lc=300),
    F("Snare & Drums | Tom Plate", STUDIO, 1.8, 22, pd=5, lc=100),
    F("Snare & Drums | Kit Ambience Plate", LIGHT, 1.0, 15, pd=10, lc=150),
    F("Snare & Drums | Clap Plate", SILVER, 1.4, 25, pd=0, lc=300),
    F("Snare & Drums | Overhead Air", GOLD, 1.0, 12, pd=10, lc=300, eh=2),
    F("Snare & Drums | Drum Bus Plate", STEEL, 1.2, 12, pd=10, lc=150),
    F("Snare & Drums | Funk Snare Plate", STEEL, 1.1, 22, pd=10, lc=200, inp=TAPEIN),
    F("Snare & Drums | Hip-Hop Snare Tail", STUDIO, 1.5, 22, pd=0, lc=200, hc=8000),
    F("Snare & Drums | Trap Clap Wash", SILVER, 2.2, 22, pd=0, lc=300),
    F("Snare & Drums | Percussion Sparkle", GOLD, 1.3, 18, pd=15, lc=400),
    F("Snare & Drums | Dark Snare Plate", STUDIO, 1.7, 22, pd=5, lc=150, hc=6000),
    # Instruments
    F("Instruments | Acoustic Guitar Plate", STEEL, 1.6, 18, pd=20, lc=120),
    F("Instruments | Electric Guitar Plate", STUDIO, 1.8, 20, pd=15, lc=120),
    F("Instruments | Piano Plate", STEEL, 2.4, 20, pd=20, lc=80),
    F("Instruments | Electric Piano Plate", GOLD, 1.8, 22, pd=15, lc=120),
    F("Instruments | Strings Plate", IRON, 2.8, 22, pd=25, lc=80),
    F("Instruments | Brass Plate", STEEL, 1.6, 18, pd=20, lc=150),
    F("Instruments | Organ Plate", STUDIO, 2.2, 20, pd=10, lc=100),
    F("Instruments | Synth Lead Plate", SOLID, 1.6, 20, pd=30, lc=200, inp=SS),
    F("Instruments | Pad Plate", SILVER, 3.5, 30, pd=20, lc=150),
    F("Instruments | Bass Plate (Sides Only Feel)", LIGHT, 0.8, 8, pd=0, lc=300),
    F("Instruments | Harp Plate", GOLD, 2.4, 22, pd=20, lc=150),
    F("Instruments | Mallets Plate", SILVER, 2.2, 22, pd=15, lc=200),
    F("Instruments | Saxophone Plate", STEEL, 1.9, 20, pd=25, lc=120),
    F("Instruments | Flute Plate", GOLD, 2.2, 20, pd=25, lc=200),
    F("Instruments | Pluck Plate", LIGHT, 1.2, 20, pd=10, lc=200),
    F("Instruments | Bell Plate", SILVER, 3.0, 25, pd=10, lc=250),
    F("Instruments | Accordion Tape Plate", STEEL, 1.8, 20, pd=15, lc=150, inp=TAPEIN),
    # Long / Ambient
    F("Long / Ambient | Big Steel Hall", STEEL, 5.0, 30, pd=40, lc=100),
    F("Long / Ambient | Iron Cathedral", IRON, 7.0, 32, pd=50, lc=80),
    F("Long / Ambient | Silver Haze", SILVER, 6.0, 32, pd=40, lc=150),
    F("Long / Ambient | Gold Foil Cloud", GOLD, 5.5, 30, pd=30, lc=150),
    F("Long / Ambient | Ambient Guitar Plate", STUDIO, 6.0, 30, pd=40, lc=100),
    F("Long / Ambient | Slow-Moving Plate", STEEL, 5.0, 30, pd=30, mod=70, mr=0.3),
    F("Long / Ambient | Wide Ambient Plate", LIGHT, 4.5, 30, pd=30, width=150),
    F("Long / Ambient | Dark Ambient Plate", IRON, 6.0, 30, pd=30, hc=5000),
    F("Long / Ambient | Shimmer-Edge Plate", SILVER, 8.0, 30, pd=40, eh=3),
    F("Long / Ambient | Cinematic Iron", IRON, 9.0, 35, pd=40),
    F("Long / Ambient | Piano Hall Plate", STEEL, 4.0, 25, pd=30),
    F("Long / Ambient | Swell Pad Plate", GOLD, 6.0, 35, pd=60),
    F("Long / Ambient | Distant Plate", STUDIO, 5.0, 30, pd=80, hc=6000),
    F("Long / Ambient | Lengthening Ambient (auto-decay)", SILVER, 3.0, 30, pd=30, ad=100, at=-36),
    F("Long / Ambient | Mono Source Wide Plate", STEEL, 4.0, 30, pd=30, mono=True, width=150),
    F("Long / Ambient | Clean Long Plate", LIGHT, 5.0, 30, pd=30, inp=CLEAN),
    F("Long / Ambient | Tape-Fed Long Plate", STEEL, 5.0, 30, pd=30, inp=TAPEIN),
    # Infinite / Pads
    F("Infinite / Pads | 30 Second Steel", STEEL, 30.0, 40, pd=20),
    F("Infinite / Pads | 60 Second Iron", IRON, 60.0, 40, pd=20),
    F("Infinite / Pads | Endless Silver", SILVER, 45.0, 40, pd=20),
    F("Infinite / Pads | Endless Gold", GOLD, 40.0, 40, pd=20),
    F("Infinite / Pads | Darkening Infinite Pad", STEEL, 40.0, 40, pd=20, dark=True, hc=8000),
    F("Infinite / Pads | Drone Plate", IRON, 50.0, 45, pd=0, lc=60),
    F("Infinite / Pads | Glass Pad", SILVER, 25.0, 40, pd=10, eh=2),
    F("Infinite / Pads | Warm Pad Plate", STUDIO, 30.0, 40, pd=10, hc=6000),
    F("Infinite / Pads | Wide Infinite", LIGHT, 30.0, 40, width=150),
    F("Infinite / Pads | Solid-State Pad", SOLID, 25.0, 40, inp=SS),
    F("Infinite / Pads | Tape Pad Plate", STEEL, 25.0, 40, inp=TAPEIN),
    F("Infinite / Pads | Slow Mod Pad", GOLD, 30.0, 40, mod=80, mr=0.25),
    F("Infinite / Pads | Fast Mod Pad", SILVER, 30.0, 40, mod=60, mr=4.0),
    F("Infinite / Pads | Sub-Rich Pad", IRON, 35.0, 40, el=3),
    F("Infinite / Pads | Airy Pad", GOLD, 25.0, 40, eh=3, lc=200),
    F("Infinite / Pads | Ducked Infinite (blooms in the gaps)", STEEL, 20.0, 40, ad=-80, at=-30),
    # Vintage
    F("Vintage | 1960s Steel Plate", STEEL, 2.2, 22, pd=0, inp=TUBE, drv=60, hc=10000),
    F("Vintage | 1970s Gold Foil", GOLD, 2.0, 22, pd=0, inp=TUBE, drv=55),
    F("Vintage | Motor City-Style Plate", STUDIO, 1.8, 24, pd=0, inp=TUBE, drv=65, hc=9000),
    F("Vintage | 1980s Solid-State", SOLID, 2.4, 24, pd=10, inp=SS, drv=55, eh=2),
    F("Vintage | Tape Echo Into Plate", STEEL, 2.0, 24, pd=120, inp=TAPEIN, drv=60),
    F("Vintage | Slapback Plate", STEEL, 1.4, 22, pd=110, inp=TAPEIN),
    F("Vintage | Rockabilly Plate", STEEL, 1.6, 24, pd=90, inp=TUBE),
    F("Vintage | Surf Plate", STUDIO, 2.6, 28, pd=0, inp=TUBE, drv=70),
    F("Vintage | Lo-Fi Radio Plate", STEEL, 1.6, 22, lc=500, hc=4000, inp=TAPEIN),
    F("Vintage | Old Church Plate", IRON, 3.5, 26, pd=20, hc=7000),
    F("Vintage | Doo-Wop Group Plate", STEEL, 2.4, 28, inp=TUBE),
    F("Vintage | Psychedelic Plate", GOLD, 3.0, 30, mod=80, mr=1.6),
    F("Vintage | Soul Horns Plate", STUDIO, 1.7, 20, inp=TUBE),
    F("Vintage | Disco Strings Plate", SILVER, 2.5, 24, inp=SS),
    F("Vintage | Steep-Cut Vintage Plate", STEEL, 2.0, 22, lc=300, hc=7000, ls=2, hs=2),
    F("Vintage | Gentle-Slope Plate", STEEL, 2.0, 22, lc=150, hc=9000, ls=0, hs=0),
    # Tight / Short
    F("Tight / Short | Short Steel", STEEL, 0.6, 15, pd=5),
    F("Tight / Short | Tiny Gold", GOLD, 0.5, 15, pd=5),
    F("Tight / Short | Room-Like Plate", LIGHT, 0.4, 14, pd=8),
    F("Tight / Short | Vocal Tuck", LIGHT, 0.7, 12, pd=20, lc=200),
    F("Tight / Short | Snare Snap", SOLID, 0.5, 20, pd=0, lc=250, inp=SS),
    F("Tight / Short | Guitar Ambience", STUDIO, 0.6, 15, pd=10),
    F("Tight / Short | Keys Ambience", STEEL, 0.7, 15, pd=10),
    F("Tight / Short | Short Iron Thump", IRON, 0.5, 12, pd=0, lc=60),
    F("Tight / Short | Short Silver", SILVER, 0.6, 15, pd=10),
    F("Tight / Short | Dialogue Plate", LIGHT, 0.4, 10, pd=5, lc=200, hc=8000),
    F("Tight / Short | Short Ducked Plate", STEEL, 0.9, 18, ad=-60, at=-30),
    F("Tight / Short | Bright Short Plate", GOLD, 0.6, 15, eh=3),
    F("Tight / Short | Dark Short Plate", STUDIO, 0.7, 15, hc=5000),
    F("Tight / Short | Wide Short Plate", LIGHT, 0.6, 15, width=150),
    F("Tight / Short | Mono Short Plate", STEEL, 0.6, 15, width=0),
    F("Tight / Short | Clean Short Plate", LIGHT, 0.5, 14, inp=CLEAN),
    F("Tight / Short | Podcast Presence", LIGHT, 0.35, 8, pd=5, lc=250, hc=9000),
    F("Vocal Plates | Harmony Stack Plate", GOLD, 2.2, 26, pd=25, lc=200, width=140),
    F("Vocal Plates | Iron Gospel Plate", IRON, 2.8, 24, pd=30, lc=140),
    F("Instruments | Banjo & Mandolin Plate", GOLD, 1.4, 18, pd=15, lc=200),
    F("Instruments | Clean Guitar Silver", SILVER, 2.0, 20, pd=20, lc=150),
]
LITE = []
for cat, items in [
    ("Vocal", [("Vocal Plate", 1.8, 120, 30, 22), ("Short Vocal", 0.9, 150, 20, 16), ("Long Ballad", 3.2, 120, 30, 24), ("Rap Tuck", 0.7, 200, 20, 14), ("Lush Vocal", 2.6, 150, 50, 26)]),
    ("Drums", [("Snare Plate", 1.5, 200, 20, 25), ("Tight Snare", 0.8, 250, 15, 22), ("Big 80s Snare", 2.4, 200, 30, 30), ("Kit Glue", 1.0, 150, 20, 12), ("Clap Tail", 1.8, 300, 30, 24)]),
    ("Instruments", [("Acoustic Guitar", 1.6, 120, 30, 18), ("Electric Guitar", 1.8, 120, 30, 20), ("Piano", 2.4, 80, 30, 20), ("Keys", 1.8, 120, 30, 22), ("Strings", 2.8, 80, 40, 22)]),
    ("Long", [("Long Steel", 5.0, 100, 40, 30), ("Huge Steel", 8.0, 100, 50, 32), ("Ambient Plate", 6.0, 150, 60, 32), ("Swell", 7.0, 120, 50, 35), ("Ten Seconds", 10.0, 100, 50, 35)]),
    ("Short", [("Short Steel", 0.6, 100, 20, 15), ("Room-Like", 0.4, 120, 20, 14), ("Tuck", 0.5, 200, 15, 12), ("Snap", 0.5, 250, 15, 18), ("Dialogue", 0.4, 200, 10, 10)]),
    ("Movement", [("Slow Mod", 2.5, 120, 80, 25), ("Wobbly Plate", 2.0, 120, 100, 25), ("Still Plate", 2.0, 120, 0, 25), ("Gentle Mod", 2.0, 120, 50, 25), ("Shimmer Mod", 3.0, 200, 70, 28)]),
]:
    for nm, dec, lc, mod, mix in items:
        LITE.append(dict(name=cat + " | " + nm, decay=dec, lowCut=lc, mod=mod, mix=mix))

# ----------------------------------------------------------------------------------------------- Radiant / Pre / Maw / Lite
MIX60, BROAD50, LINEAMP = range(3)
RAD = []
def Rd(name, arch, inp, bass=0, treble=0, out=0, mix=100, mic=False, noisy=False, os_=2, ms=0):
    RAD.append(dict(name=name, arch=arch, input=inp, bass=bass, treble=treble, output=out, mix=mix, mic=mic, noisy=noisy, os=os_, ms=ms))
for a, an in ((MIX60, "60s Mixer"), (BROAD50, "50s Broadcast"), (LINEAMP, "Line Amp")):
    Rd("Vocal | %s Warm Lift" % an, a, 4, bass=-1, treble=2)
    Rd("Vocal | %s Gritty Vocal" % an, a, 14, bass=-2, treble=1, mix=80)
    Rd("Vocal | %s Intimate Body" % an, a, 6, bass=2, treble=-1)
    Rd("Vocal | %s Radio Presence" % an, a, 10, bass=-4, treble=4)
    Rd("Drums | %s Kick Weight" % an, a, 8, bass=4, treble=-2)
    Rd("Drums | %s Snare Crack" % an, a, 12, bass=-2, treble=3)
    Rd("Drums | %s Bus Glue" % an, a, 6, bass=1, treble=1)
    Rd("Drums | %s Parallel Smash" % an, a, 20, bass=2, treble=2, mix=35)
    Rd("Bass | %s DI Warmth" % an, a, 8, bass=2, treble=-1)
    Rd("Bass | %s Growl" % an, a, 16, bass=0, treble=3, mix=70)
    Rd("Keys | %s Piano Glow" % an, a, 5, bass=1, treble=1)
    Rd("Keys | %s Organ Push" % an, a, 14, bass=2, treble=0)
    Rd("Guitar | %s Clean Sparkle" % an, a, 4, treble=3)
    Rd("Guitar | %s Crunch" % an, a, 18, bass=-2, treble=2)
    Rd("Synth | %s Analog Heat" % an, a, 10, bass=1, treble=1)
    Rd("Synth | %s Lead Bite" % an, a, 16, bass=-3, treble=4, mix=80)
    Rd("Mix Bus | %s Gentle Color" % an, a, 2, bass=0.5, treble=0.5, os_=3)
    Rd("Mix Bus | %s Mid Heat (M/S)" % an, a, 6, ms=2, os_=3)
    Rd("Mix Bus | %s Side Air (M/S)" % an, a, 4, treble=3, ms=3, os_=3)
    Rd("Lo-Fi | %s Noisy Old Desk" % an, a, 12, bass=-3, treble=-6, noisy=True)
    Rd("Lo-Fi | %s Mic Input Overload" % an, a, 14, mic=True, bass=-2, treble=-2)
    Rd("Mix Bus | %s Tape-Era Bus" % an, a, 4, bass=1, treble=-1, os_=3)
    Rd("Vocal | %s Back Vocal Thickener" % an, a, 8, bass=1, treble=1, mix=60)
    Rd("Drums | %s Room Mics Heat" % an, a, 15, bass=1, treble=-2)
    Rd("Keys | %s Electric Piano Drive" % an, a, 12, bass=0, treble=2)
    Rd("Guitar | %s Acoustic Body" % an, a, 3, bass=2, treble=1)
    Rd("Bass | %s Sub Tightener" % an, a, 6, bass=-3, treble=2)
    Rd("Synth | %s Pad Warmth" % an, a, 5, bass=1, treble=-2)
    Rd("Drums | %s Overhead Silk" % an, a, 3, bass=-2, treble=2)
    Rd("Lo-Fi | %s Broadcast Radio" % an, a, 10, bass=-8, treble=-6)
PRE = []
for nm, g, out, mix, bias, noisy in [
    ("Vocal | Clean Mic Pre", 4, 0, 100, False, False), ("Vocal | Warm Mic Pre", 10, 0, 100, False, False), ("Vocal | Hot Mic Pre", 16, 0, 100, False, False),
    ("Vocal | Choppy Bias Vocal", 6, 0, 60, True, False), ("Vocal | Vintage Noisy Pre", 10, 0, 100, False, True), ("Vocal | Parallel Hot Pre", 22, 0, 35, False, False),
    ("Drums | Kick Pre Push", 12, 0, 100, False, False), ("Drums | Snare Pre Bite", 16, 0, 100, False, False), ("Drums | Room Pre Crush", 24, 0, 100, False, False),
    ("Drums | Choppy Bias Drums", 10, 0, 50, True, False), ("Drums | Parallel Pre", 20, 0, 40, False, False), ("Drums | Overhead Pre", 6, 0, 100, False, False),
    ("Bass | DI Pre", 8, 0, 100, False, False), ("Bass | Hot Bass Pre", 16, 0, 100, False, False), ("Bass | Choppy Bass Fuzz", 8, 0, 50, True, False),
    ("Bass | Parallel Bass Pre", 20, 0, 40, False, False), ("Guitar | Clean Guitar Pre", 4, 0, 100, False, False), ("Guitar | Edge Pre", 14, 0, 100, False, False),
    ("Guitar | Fuzzed-Out Pre", 26, 0, 100, False, False), ("Guitar | Choppy Gated Guitar", 8, 0, 70, True, False), ("Keys | Piano Pre", 6, 0, 100, False, False),
    ("Keys | Organ Pre Drive", 16, 0, 100, False, False), ("Keys | Electric Piano Pre", 12, 0, 100, False, False), ("Keys | Choppy Keys", 6, 0, 50, True, False),
    ("Synth | Synth Pre Warmth", 8, 0, 100, False, False), ("Synth | Synth Pre Grit", 18, 0, 100, False, False), ("Synth | Choppy Synth", 8, 0, 60, True, False),
    ("Synth | Parallel Synth Pre", 22, 0, 35, False, False), ("Lo-Fi | Noisy Old Pre", 12, 0, 100, False, True), ("Lo-Fi | Broken Bias Pre", 10, 0, 100, True, True),
    ("Lo-Fi | Overdriven Noisy Pre", 22, 0, 100, False, True), ("Lo-Fi | Starved Tube", 4, 0, 100, True, False), ("Mix Bus | Gentle Pre Color", 2, 0, 100, False, False),
    ("Mix Bus | Pre Glue", 5, 0, 100, False, False), ("Mix Bus | Parallel Pre Heat", 14, 0, 30, False, False), ("Mix Bus | Choppy Parallel", 8, 0, 25, True, False),
    ("Vocal | Rap Vocal Pre", 12, 0, 100, False, False), ("Vocal | Whisper Pre", 2, 0, 100, False, False), ("Drums | 808 Pre Harmonics", 14, 0, 100, False, False),
    ("Guitar | Acoustic Pre", 3, 0, 100, False, False)]:
    PRE.append(dict(name=nm, gain=g, output=out, mix=mix, bias=bias, noisy=noisy))
MAW = []
def Mw(name, crush, crunch, dark=20000, mix=100, rel=1.7, stretch=100, fast=False, os_=2):
    MAW.append(dict(name=name, crush=crush, crunch=crunch, darkness=dark, mix=mix, release=rel, stretch=stretch, fast=fast, os=os_))
for i, (nm, cr, cn, dk, mx, rl, st, fs) in enumerate([
    ("Drum Room Crush | Room Mics Destroyed", 80, 40, 12000, 100, 1.7, 100, False), ("Drum Room Crush | Pumping Room", 70, 20, 16000, 100, 0.4, 0, True),
    ("Drum Room Crush | Dark Room Squash", 75, 30, 5000, 100, 1.7, 100, False), ("Drum Room Crush | Breathing Kit", 60, 20, 14000, 100, 0.8, 30, False),
    ("Drum Room Crush | Mono Room Slam", 85, 50, 10000, 100, 1.0, 60, False), ("Drum Room Crush | Fast Release Room", 70, 30, 14000, 100, 0.15, 0, False),
    ("Drum Room Crush | Long Stretch Room", 80, 30, 12000, 100, 2.5, 100, False), ("Drum Room Crush | Overhead Crush", 55, 20, 18000, 100, 0.6, 30, False),
    ("Drum Room Crush | Trash Mic", 95, 70, 8000, 100, 0.5, 50, False), ("Drum Room Crush | Glue Crush", 45, 15, 20000, 100, 1.2, 50, False),
    ("Drum Room Crush | Snare Room Bloom", 75, 25, 9000, 100, 3.0, 100, False), ("Drum Room Crush | Kick Room Pump", 65, 20, 6000, 100, 0.25, 0, True),
    ("Drum Room Crush | Crunchy Ambience", 70, 60, 11000, 100, 1.0, 50, False), ("Drum Room Crush | Vintage Kit Squash", 60, 35, 10000, 100, 1.7, 100, False),
    ("Drum Room Crush | Tight Room", 50, 15, 16000, 100, 0.1, 0, True), ("Drum Room Crush | Huge Room", 90, 40, 13000, 100, 4.0, 100, False),
    ("Drum Room Crush | Darkest Room", 85, 30, 2500, 100, 1.7, 100, False), ("Drum Room Crush | Glued Breakbeat", 55, 35, 12000, 100, 0.5, 30, False),
    ("Drum Room Crush | Live Room Energy", 65, 25, 15000, 100, 0.9, 40, False), ("Drum Room Crush | Max Everything", 100, 100, 9000, 100, 1.7, 100, False),
]):
    Mw(nm, cr, cn, dk, mx, rl, st, fs)
for nm, cr, cn, dk, mx in [("Parallel Smash | Drum Bus 30%", 85, 40, 14000, 30), ("Parallel Smash | Drum Bus 50%", 80, 30, 14000, 50), ("Parallel Smash | NY-Style Kit 25%", 90, 50, 12000, 25),
                          ("Parallel Smash | Vocal Parallel 20%", 75, 30, 10000, 20), ("Parallel Smash | Bass Parallel 30%", 80, 40, 6000, 30), ("Parallel Smash | Mix Bus 15%", 70, 20, 16000, 15),
                          ("Parallel Smash | Snare Smash 35%", 90, 50, 10000, 35), ("Parallel Smash | Room Blend 40%", 85, 35, 12000, 40), ("Parallel Smash | Dark Parallel", 80, 30, 4000, 30),
                          ("Parallel Smash | Bright Parallel", 80, 30, 20000, 30), ("Parallel Smash | Crunch Parallel", 85, 80, 12000, 25), ("Parallel Smash | Gentle Parallel", 60, 15, 16000, 30),
                          ("Parallel Smash | Guitar Parallel", 80, 40, 9000, 30), ("Parallel Smash | Keys Parallel", 75, 30, 12000, 25), ("Parallel Smash | Synth Parallel", 85, 50, 14000, 30),
                          ("Parallel Smash | 808 Parallel", 85, 50, 5000, 30), ("Parallel Smash | Breakbeat Parallel", 90, 60, 11000, 35), ("Parallel Smash | Clap Parallel", 85, 40, 13000, 30)]:
    Mw(nm, cr, cn, dk, mx)
for nm, cr, cn, dk, mx, rl in [("Vocal Grit | Rap Vocal Grit", 50, 40, 16000, 100, 0.4), ("Vocal Grit | Rock Vocal Crush", 65, 50, 12000, 100, 0.6), ("Vocal Grit | Vintage PA Vocal", 60, 40, 8000, 100, 1.0),
                               ("Vocal Grit | Megaphone Vocal", 80, 70, 5000, 100, 0.5), ("Vocal Grit | Gentle Vocal Level", 35, 10, 18000, 100, 0.8), ("Vocal Grit | Parallel Vocal Grit", 70, 50, 12000, 35, 0.6),
                               ("Vocal Grit | Ad-Lib Smash", 85, 60, 9000, 100, 0.3), ("Vocal Grit | Background Vocal Glue", 45, 20, 14000, 100, 1.0), ("Vocal Grit | Spoken Word Crush", 55, 25, 10000, 100, 0.7),
                               ("Vocal Grit | Screamed Vocal", 90, 80, 9000, 100, 0.4), ("Vocal Grit | Lo-Fi Vocal Crush", 75, 60, 4500, 100, 0.8), ("Vocal Grit | Breathy Vocal Lift", 45, 15, 18000, 100, 0.5),
                               ("Vocal Grit | Gospel Vocal Push", 50, 30, 15000, 100, 1.0), ("Vocal Grit | Vocal Level Fast", 50, 20, 17000, 100, 0.12), ("Vocal Grit | Dark Vocal Squash", 65, 35, 6000, 100, 1.0),
                               ("Vocal Grit | Bright Vocal Grit", 55, 45, 20000, 100, 0.5), ("Vocal Grit | Vocal Chain Glue", 40, 20, 18000, 100, 1.2), ("Vocal Grit | Shouted Hook", 80, 55, 10000, 100, 0.4)]:
    Mw(nm, cr, cn, dk, mx, rl, 50)
for nm, cr, cn, dk, mx, rl in [("Bass | Bass Lock", 55, 30, 8000, 100, 0.5), ("Bass | Fuzz Bass Crush", 80, 80, 6000, 100, 0.4), ("Bass | Dark Bass Squash", 65, 30, 3000, 100, 0.8),
                               ("Bass | Funk Bass Snap", 50, 25, 12000, 100, 0.2), ("Bass | Synth Bass Pump", 70, 40, 10000, 100, 0.15), ("Bass | 808 Crush", 75, 60, 5000, 100, 0.4),
                               ("Bass | Upright Body Crush", 45, 20, 7000, 100, 1.0), ("Bass | Dub Bass Lock", 60, 30, 3500, 100, 1.2), ("Bass | Parallel Bass Crush", 85, 50, 5000, 30, 0.5),
                               ("Bass | Bass Grit", 60, 60, 9000, 100, 0.5), ("Bass | Reese Squash", 70, 50, 12000, 100, 0.4), ("Bass | Gentle Bass Level", 35, 10, 10000, 100, 0.8),
                               ("Bass | Bass DI Crunch", 55, 45, 9000, 100, 0.6), ("Bass | Sub Glue", 40, 15, 4000, 100, 1.0), ("Bass | Rock Bass Crush", 65, 40, 8000, 100, 0.6),
                               ("Bass | Max Bass", 90, 70, 6000, 100, 0.5), ("Bass | Pick Bass Crush", 60, 35, 11000, 100, 0.3), ("Bass | Warm Bass Squeeze", 50, 25, 6000, 100, 0.9)]:
    Mw(nm, cr, cn, dk, mx, rl, 50)
for nm, cr, cn, dk, mx in [("Lo-Fi | Walkie Crush", 90, 80, 3000, 100), ("Lo-Fi | AM Crush", 80, 60, 4500, 100), ("Lo-Fi | Cassette Squash", 70, 40, 6000, 100),
                          ("Lo-Fi | Tin Can", 95, 90, 2500, 100), ("Lo-Fi | Old Sampler Crush", 75, 70, 7000, 100), ("Lo-Fi | Lo-Fi Drum Bus", 80, 60, 5000, 100),
                          ("Lo-Fi | Dusty Keys Crush", 65, 40, 4000, 100), ("Lo-Fi | Broken Speaker", 100, 100, 3500, 100), ("Lo-Fi | Muffled Pump", 85, 30, 1500, 100),
                          ("Lo-Fi | Gritty Lo-Fi Vocal", 75, 70, 4000, 100), ("Lo-Fi | Phone Crush", 90, 70, 3000, 100), ("Lo-Fi | Lo-Fi Parallel", 85, 60, 4000, 40),
                          ("Lo-Fi | Basement Tape", 70, 50, 5000, 100), ("Lo-Fi | Overdriven PA", 95, 85, 6000, 100), ("Lo-Fi | Hiss-Free Lo-Fi", 60, 40, 5000, 100),
                          ("Lo-Fi | Dark Lo-Fi Loop", 80, 50, 2000, 100), ("Lo-Fi | Crunchy Loop", 85, 80, 8000, 100), ("Lo-Fi | Radio Squash", 75, 60, 4500, 100)]:
    Mw(nm, cr, cn, dk, mx)
for nm, cr, cn, dk, mx, rl, st in [("Sound Design | Infinite Sustain", 100, 40, 12000, 100, 30.0, 100), ("Sound Design | Reverse-Feel Swell", 95, 30, 10000, 100, 8.0, 100),
                                   ("Sound Design | Gate Pump", 90, 30, 14000, 100, 0.05, 0), ("Sound Design | Ambient Noise Riser", 100, 60, 8000, 100, 15.0, 100),
                                   ("Sound Design | Distortion Wall", 100, 100, 9000, 100, 1.7, 100), ("Sound Design | Subtle Breathing", 70, 10, 16000, 100, 0.3, 0),
                                   ("Sound Design | Crushed Foley", 85, 50, 10000, 100, 1.0, 50), ("Sound Design | Room Tone Lift", 95, 20, 12000, 100, 5.0, 100),
                                   ("Sound Design | Darkness Sweep Bed", 90, 40, 1200, 100, 2.0, 100), ("Sound Design | Max Stretch", 85, 30, 14000, 100, 1.7, 100),
                                   ("Sound Design | Pump Machine", 95, 50, 12000, 100, 0.08, 0), ("Sound Design | Grit Texture", 80, 90, 7000, 100, 0.8, 50),
                                   ("Sound Design | Cymbal Swell", 90, 20, 16000, 100, 10.0, 100), ("Sound Design | Crunchy Impact", 100, 80, 8000, 100, 0.5, 50),
                                   ("Sound Design | Tail Extender", 95, 20, 14000, 100, 20.0, 100), ("Sound Design | Breath Machine", 85, 30, 11000, 100, 0.2, 0),
                                   ("Sound Design | Slow Bloom Pad", 90, 15, 10000, 100, 12.0, 100), ("Sound Design | Stutter Pump", 95, 40, 13000, 100, 0.06, 0)]:
    Mw(nm, cr, cn, dk, mx, rl, st)
LITEM = []
for nm, cr, cn in [("Drums | Room Crush", 80, 40), ("Drums | Kit Glue", 45, 15), ("Drums | Snare Smash", 85, 50), ("Drums | Pumping Kit", 70, 20), ("Drums | Max Crush", 100, 60),
                   ("Vocal | Vocal Grit", 55, 40), ("Vocal | Gentle Level", 35, 10), ("Vocal | Rap Crush", 65, 45), ("Vocal | Megaphone-ish", 80, 70), ("Vocal | Background Glue", 45, 20),
                   ("Bass | Bass Lock", 55, 30), ("Bass | Fuzz Bass", 80, 80), ("Bass | 808 Grit", 75, 60), ("Bass | Gentle Bass", 35, 10), ("Bass | Funk Snap", 50, 25),
                   ("Keys | Organ Crush", 70, 50), ("Keys | Piano Glue", 40, 15), ("Keys | Lo-Fi Keys", 75, 60), ("Keys | Electric Piano Bark", 60, 45), ("Keys | Pad Squash", 65, 20),
                   ("Guitar | Guitar Crush", 70, 50), ("Guitar | Acoustic Level", 40, 10), ("Guitar | Fuzz Wall", 90, 90), ("Guitar | Funk Snap", 55, 25), ("Guitar | Room Guitar", 75, 30),
                   ("Lo-Fi | Tin Can", 95, 90), ("Lo-Fi | Walkie", 90, 80), ("Lo-Fi | Cassette Squash", 70, 40), ("Lo-Fi | Broken Speaker", 100, 100), ("Lo-Fi | Gentle Lo-Fi", 60, 30)]:
    LITEM.append(dict(name=nm, crush=cr, crunch=cn))

def write_table(path, presets, wb=None, jr=None, fp=None, fl=None, rd=None, pre=None, maw=None, mawl=None):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111.py - do not edit.\n#pragma once\nstruct CinderPresetRow { const char* name; int model, modelB; float blend, drive, bias, dyn, lc, thump, tone, hc; int steep; float mix; int punish, ms, os, linear, autoGain; };\n')
        f.write('static const CinderPresetRow cinderPresets[] = {\n')
        for p in presets:
            f.write('    { %s, %d, %d, %g, %g, %g, %g, %g, %g, %g, %g, %d, %g, %d, %d, %d, %d, %d },\n' % (
                json.dumps(p['name']), p['model'], p['modelB'], p['blend'], p['drive'], p['bias'], p['dynamics'], p['lowCut'], p['thump'],
                p['tone'], p['highCut'], int(p['steep']), p['mix'], int(p['punish']), p['ms'], p['oversample'], int(p['linear']), int(p['autoGain'])))
        f.write('};\n')
        f.write('struct WingbeatPresetRow { const char* name; int style, mode, syncL, syncR; float time, timeR, feedback, mix, saturation, groove, feel, width, offset, accent, warp, lowCut, highCut, wow, flutter, diffusion, duck, duckRelease; int heads, limiter, pattern, timeChange, prime, jr; };\n')
        f.write('static const WingbeatPresetRow wingbeatPresets[] = {\n')
        for p in (wb or []):
            f.write('    { %s, %d, %d, %d, %d, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %d, %d, %d, %d, %d, 0 },\n' % (
                json.dumps(p['name']), p['style'], p['mode'], p['syncL'], p['syncR'], p['time'], p['timeR'], p['feedback'], p['mix'], p['saturation'], p['groove'],
                p['feel'], p['width'], p['offset'], p['accent'], p['warp'], p['lowCut'], p['highCut'], p['wow'], p['flutter'], p['diffusion'], p['duck'], p['duckRelease'],
                p['heads'], p['limiter'], p['pattern'], p['timeChange'], int(p['prime'])))
        jrmap = [1, 3, 4, 5, 7, 14, 10, 30]
        for p in (jr or []):
            mode = [0, 1, 2][p['mode']]
            f.write('    { %s, %d, %d, %d, %d, %g, %g, %g, %g, %g, 0, 0, %g, %g, 0, 0, 20, %g, 100, 100, 0, 0, 250, 1, 1, 0, %d, 0, 1 },\n' % (
                json.dumps(p['name']), jrmap[p['style']], mode, p['sync'], p['sync'], p['time'], p['time'], p['feedback'], p['mix'], p['saturation'],
                130 if p['mode'] == 1 else 100, 17 if p['mode'] == 1 else 0, p['tone'], 2 if p['change'] == 1 else 0))
        f.write('};\n')
        f.write('struct FoilPresetRow { const char* name; int plate; float decay, mix, preDelay, lowCut, highCut, mod, modRate, width, balance, eqLow, eqHigh, autoDecay, autoThreshold, drive; int input, lowSlope, highSlope, darkening, monoIn, lite; };\n')
        f.write('static const FoilPresetRow foilPresets[] = {\n')
        for p in (fp or []):
            f.write('    { %s, %d, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %d, %d, %d, %d, %d, 0 },\n' % (json.dumps(p['name']), p['plate'], p['decay'], p['mix'],
                    p['preDelay'], p['lowCut'], p['highCut'], p['mod'], p['modRate'], p['width'], p['balance'], p['eqLow'], p['eqHigh'], p['autoDecay'], p['autoThreshold'],
                    p['drive'], p['input'], p['lowSlope'], p['highSlope'], int(p['darkening']), int(p['monoIn'])))
        for p in (fl or []):
            f.write('    { %s, 0, %g, %g, 8, %g, 12000, %g, 0.8, 100, 0, 0, 0, 0, -30, 50, 0, 1, 1, 0, 0, 1 },\n' % (json.dumps(p['name']), p['decay'], p['mix'], p['lowCut'], p['mod']))
        f.write('};\n')
        f.write('struct RadiantPresetRow { const char* name; int arch; float input, bass, treble, output, mix; int mic, noisy, os, ms, pre, bias; };\n')
        f.write('static const RadiantPresetRow radiantPresets[] = {\n')
        for p in (rd or []):
            f.write('    { %s, %d, %g, %g, %g, %g, %g, %d, %d, %d, %d, 0, 0 },\n' % (json.dumps(p['name']), p['arch'], p['input'], p['bass'], p['treble'], p['output'], p['mix'], int(p['mic']), int(p['noisy']), p['os'], p['ms']))
        for p in (pre or []):
            f.write('    { %s, 3, %g, 0, 0, 0, %g, 0, %d, 2, 0, 1, %d },\n' % (json.dumps(p['name']), p['gain'], p['mix'], int(p['noisy']), int(p['bias'])))
        f.write('};\n')
        f.write('struct MawPresetRow { const char* name; float crush, crunch, darkness, mix, release, stretch; int fast, os, lite; };\n')
        f.write('static const MawPresetRow mawPresets[] = {\n')
        for p in (maw or []):
            f.write('    { %s, %g, %g, %g, %g, %g, %g, %d, %d, 0 },\n' % (json.dumps(p['name']), p['crush'], p['crunch'], p['darkness'], p['mix'], p['release'], p['stretch'], int(p['fast']), p['os']))
        for p in (mawl or []):
            f.write('    { %s, %g, %g, 20000, 100, 1.7, 100, 0, 2, 1 },\n' % (json.dumps(p['name']), p['crush'], p['crunch']))
        f.write('};\n')

def calibrate():
    exe = '/tmp/preset_calib'
    src = os.path.join(ROOT, 'tests', 'preset_calib.cpp')
    subprocess.check_call(['g++', '-O2', '-std=c++17', '-I' + os.path.join(ROOT, 'Source'), '-I' + os.path.join(ROOT, 'tests'), src, '-o', exe])
    out = subprocess.check_output([exe], text=True)
    trims = {}
    for line in out.splitlines():
        if line.startswith('TRIM\t'):
            parts = line.split('\t')
            if len(parts) == 5: _, plug, name, trim, spread = parts
            else: _, name, trim, spread = parts; plug = 'Cinder'
            trims[(plug, name)] = (float(trim), float(spread))
    return trims

def num(v):
    s = ('%.2f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def emit_cinder(presets, trims):
    lines = ['        // Cinder props: model / modelB (0 Tape Preamp .. 9 Broadcast Limiter Amp), punishOn, steepOn, autoGainOn,',
             '        // msMode (0 stereo, 1 M/S, 2 mid only, 3 side only), oversample (0..4 = 1x..16x), linearPhaseOn. Output = measured trim.',
             '        { "Cinder", {']
    for p in presets:
        trim = max(-24.0, min(12.0, trims[('Cinder', p['name'])][0]))
        vals = ['N ("drive", %s)' % num(p['drive']), 'N ("blend", %s)' % num(p['blend']), 'N ("bias", %s)' % num(p['bias']),
                'N ("dynamics", %s)' % num(p['dynamics']), 'N ("lowCut", %s)' % num(p['lowCut']), 'N ("thump", %s)' % num(p['thump']),
                'N ("tone", %s)' % num(p['tone']), 'N ("highCut", %s)' % num(p['highCut']), 'N ("output", %s)' % num(trim), 'N ("mix", %s)' % num(p['mix'])]
        props = ['{ "model", %d }' % p['model'], '{ "modelB", %d }' % p['modelB'], '{ "punishOn", %d }' % int(p['punish']),
                 '{ "steepOn", %d }' % int(p['steep']), '{ "autoGainOn", %d }' % int(p['autoGain']), '{ "msMode", %d }' % p['ms'],
                 '{ "oversample", %d }' % p['oversample'], '{ "linearPhaseOn", %d }' % int(p['linear'])]
        lines.append('            %s (%s, { %s },\n                { %s }),' % ('A' if p['adv'] else 'B', json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    lines.append('        } },')
    return '\n'.join(lines)


def emit_wingbeat(presets, trims):
    lines = ['        // Wingbeat Echo props: mode (0 single 1 dual 2 ping-pong 3 rhythm), style (0-35), syncL / syncR (0 free, n = division n-1),',
             '        // timeChange (0 glide 1 crossfade 2 jump), heads (1-7), limiter (0 off 1 soft 2 hard), pattern, primeOn. Output = measured trim.',
             '        { "Wingbeat Echo", {']
    for p in presets:
        trim = max(-24.0, min(12.0, trims[('Wingbeat Echo', p['name'])][0]))
        vals = ['N ("%s", %s)' % (k, num(p[k2])) for k, k2 in [("time", "time"), ("timeR", "timeR"), ("feedback", "feedback"), ("mix", "mix"), ("saturation", "saturation"),
                ("groove", "groove"), ("feel", "feel"), ("width", "width"), ("offset", "offset"), ("accent", "accent"), ("warp", "warp"), ("lowCut", "lowCut"),
                ("highCut", "highCut"), ("wow", "wow"), ("flutter", "flutter"), ("diffusion", "diffusion"), ("duck", "duck"), ("duckRelease", "duckRelease")]]
        vals.append('N ("output", %s)' % num(trim))
        props = ['{ "mode", %d }' % p['mode'], '{ "style", %d }' % p['style'], '{ "syncL", %d }' % (p['syncL'] + 1), '{ "syncR", %d }' % (p['syncR'] + 1),
                 '{ "timeChange", %d }' % p['timeChange'], '{ "heads", %d }' % p['heads'], '{ "limiter", %d }' % p['limiter'], '{ "pattern", %d }' % p['pattern'],
                 '{ "primeOn", %d }' % int(p['prime'])]
        lines.append('            %s (%s, { %s },\n                { %s }),' % ('A' if p['adv'] else 'B', json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    lines.append('        } },')
    return '\n'.join(lines)

def emit_jr(presets, trims):
    lines = ['        // Wingbeat Jr. props: style (0-7), mode (0 normal 1 wide 2 ping-pong), sync (0 free, n = division n-1), timeChange (0 glide 1 stretch).',
             '        { "Wingbeat Jr.", {']
    for p in presets:
        vals = ['N ("time", %s)' % num(p['time']), 'N ("feedback", %s)' % num(p['feedback']), 'N ("mix", %s)' % num(p['mix']),
                'N ("saturation", %s)' % num(p['saturation']), 'N ("tone", %s)' % num(p['tone']),
                'N ("output", %s)' % num(max(-24.0, min(12.0, trims[('Wingbeat Jr.', p['name'])][0])))]
        props = ['{ "style", %d }' % p['style'], '{ "mode", %d }' % p['mode'], '{ "sync", %d }' % (p['sync'] + 1), '{ "timeChange", %d }' % p['change']]
        lines.append('            B (%s, { %s },\n                { %s }),' % (json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    lines.append('        } },')
    return '\n'.join(lines)

def emit_foil(presets, lite, trims):
    lines = ['        // Foil Plate props: plate (0 Steel Classic .. 6 Iron Mammoth), input (0 Tube 1 Solid State 2 Clean 3 Tape), lowSlope / highSlope (0 6, 1 12, 2 24 dB/oct),',
             '        // darkeningOn, monoInOn. Output = measured trim.',
             '        { "Foil Plate", {']
    for p in presets:
        trim = max(-24.0, min(12.0, trims[('Foil Plate', p['name'])][0]))
        vals = ['N ("%s", %s)' % (k, num(p[k])) for k in ("decay", "preDelay", "lowCut", "highCut", "mod", "modRate", "mix", "width", "balance", "eqLow", "eqHigh", "autoDecay", "autoThreshold", "drive")]
        vals.append('N ("output", %s)' % num(trim))
        props = ['{ "plate", %d }' % p['plate'], '{ "input", %d }' % p['input'], '{ "lowSlope", %d }' % p['lowSlope'], '{ "highSlope", %d }' % p['highSlope'],
                 '{ "darkeningOn", %d }' % int(p['darkening']), '{ "monoInOn", %d }' % int(p['monoIn'])]
        lines.append('            %s (%s, { %s },\n                { %s }),' % ('A' if p['adv'] else 'B', json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    lines.append('        } },')
    lines.append('        { "Foil Plate Lite", {')
    for p in lite:
        trim = max(-24.0, min(12.0, trims[('Foil Plate Lite', p['name'])][0]))
        lines.append('            B (%s, { N ("decay", %s), N ("lowCut", %s), N ("mod", %s), N ("mix", %s), N ("output", %s) }),' % (json.dumps(p['name']), num(p['decay']), num(p['lowCut']), num(p['mod']), num(p['mix']), num(trim)))
    lines.append('        } },')
    return '\n'.join(lines)

def emit_radmaw(rd, pre, maw, mawl, trims):
    L = ['        // Radiant props: arch (0 60s Tube Mixer, 1 50s Broadcast Pre, 2 Tube Line Amp), micOn, noisyOn, oversample (0-3), msMode. Trim = measured.',
         '        { "Radiant", {']
    for p in rd:
        t = max(-24.0, min(12.0, trims[('Radiant', p['name'])][0]))
        L.append('            %s (%s, { N ("input", %s), N ("bass", %s), N ("treble", %s), N ("output", %s), N ("mix", %s), N ("trim", %s) },\n                { { "arch", %d }, { "micOn", %d }, { "noisyOn", %d }, { "oversample", %d }, { "msMode", %d } }),' % (
            'A' if p['ms'] or p['os'] != 2 else 'B', json.dumps(p['name']), num(p['input']), num(p['bass']), num(p['treble']), num(p['output']), num(p['mix']), num(t), p['arch'], int(p['mic']), int(p['noisy']), p['os'], p['ms']))
    L.append('        } },')
    L.append('        { "Radiant Pre", {')
    for p in pre:
        t = max(-24.0, min(24.0, trims[('Radiant Pre', p['name'])][0]))
        L.append('            B (%s, { N ("gain", %s), N ("output", %s), N ("mix", %s) }, { { "biasOn", %d }, { "noisyOn", %d } }),' % (json.dumps(p['name']), num(p['gain']), num(t), num(p['mix']), int(p['bias']), int(p['noisy'])))
    L.append('        } },')
    L.append('        // Maw props: fastOn (fast release), keyOn, oversample. Output = measured trim.')
    L.append('        { "Maw", {')
    for p in maw:
        t = max(-24.0, min(12.0, trims[('Maw', p['name'])][0]))
        L.append('            B (%s, { N ("crush", %s), N ("crunch", %s), N ("darkness", %s), N ("mix", %s), N ("release", %s), N ("stretch", %s), N ("output", %s) },\n                { { "fastOn", %d }, { "oversample", %d } }),' % (
            json.dumps(p['name']), num(p['crush']), num(p['crunch']), num(p['darkness']), num(p['mix']), num(p['release']), num(p['stretch']), num(t), int(p['fast']), p['os']))
    L.append('        } },')
    L.append('        { "Maw Lite", {')
    for p in mawl:
        t = max(-24.0, min(12.0, trims[('Maw Lite', p['name'])][0]))
        L.append('            B (%s, { N ("crush", %s), N ("crunch", %s), N ("output", %s) }),' % (json.dumps(p['name']), num(p['crush']), num(p['crunch']), num(t)))
    L.append('        } },')
    return '\n'.join(L)

def main():
    names = [p['name'] for p in CINDER]
    assert len(names) == len(set(names)), 'duplicate preset names'
    cats = sorted(set(n.split(' | ')[0] for n in names))
    print('Cinder: %d presets in %d categories: %s' % (len(CINDER), len(cats), ', '.join(cats)))
    for lst, nm in ((WINGBEAT, 'Wingbeat Echo'), (JR, 'Wingbeat Jr.')):
        names2 = [p['name'] for p in lst]
        assert len(names2) == len(set(names2)), 'duplicate preset names in ' + nm
        print('%s: %d presets in %d categories' % (nm, len(lst), len(set(n.split(' | ')[0] for n in names2))))
    for lst, nm in ((FOIL, 'Foil Plate'), (LITE, 'Foil Plate Lite')):
        names2 = [p['name'] for p in lst]
        assert len(names2) == len(set(names2)), 'duplicate preset names in ' + nm
        print('%s: %d presets in %d categories' % (nm, len(lst), len(set(n.split(' | ')[0] for n in names2))))
    for lst, nm in ((RAD, 'Radiant'), (PRE, 'Radiant Pre'), (MAW, 'Maw'), (LITEM, 'Maw Lite')):
        names2 = [p['name'] for p in lst]
        assert len(names2) == len(set(names2)), 'duplicate preset names in ' + nm
        print('%s: %d presets in %d categories' % (nm, len(lst), len(set(n.split(' | ')[0] for n in names2))))
    write_table(os.path.join(ROOT, 'tests', 'presets111_table.h'), CINDER, WINGBEAT, JR, FOIL, LITE, RAD, PRE, MAW, LITEM)
    trims = calibrate()
    missing = [n for n in names if ('Cinder', n) not in trims]
    assert not missing, missing
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111.py - factory presets for the 1.11 Color & Space / Voice plug-ins. Do not edit by hand.\n')
        f.write('// Included inside SignaturePresets::bank() (SignaturePresets.h), after SignaturePresets181fx.inl.\n')
        f.write(emit_cinder(CINDER, trims) + '\n')
        f.write(emit_wingbeat(WINGBEAT, trims) + '\n')
        f.write(emit_jr(JR, trims) + '\n')
        f.write(emit_foil(FOIL, LITE, trims) + '\n')
        f.write(emit_radmaw(RAD, PRE, MAW, LITEM, trims) + '\n')
    worst = max(trims.values(), key=lambda t: t[1])
    print('wrote', out, '- widest spread between the three test signals: %.2f dB' % worst[1])

if __name__ == '__main__':
    main()
