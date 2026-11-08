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

def write_table(path, presets):
    with open(path, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111.py - do not edit.\n#pragma once\nstruct CinderPresetRow { const char* name; int model, modelB; float blend, drive, bias, dyn, lc, thump, tone, hc; int steep; float mix; int punish, ms, os, linear, autoGain; };\n')
        f.write('static const CinderPresetRow cinderPresets[] = {\n')
        for p in presets:
            f.write('    { %s, %d, %d, %g, %g, %g, %g, %g, %g, %g, %g, %d, %g, %d, %d, %d, %d, %d },\n' % (
                json.dumps(p['name']), p['model'], p['modelB'], p['blend'], p['drive'], p['bias'], p['dynamics'], p['lowCut'], p['thump'],
                p['tone'], p['highCut'], int(p['steep']), p['mix'], int(p['punish']), p['ms'], p['oversample'], int(p['linear']), int(p['autoGain'])))
        f.write('};\n')

def calibrate():
    exe = '/tmp/preset_calib'
    src = os.path.join(ROOT, 'tests', 'preset_calib.cpp')
    subprocess.check_call(['g++', '-O2', '-std=c++17', '-I' + os.path.join(ROOT, 'Source'), '-I' + os.path.join(ROOT, 'tests'), src, '-o', exe])
    out = subprocess.check_output([exe], text=True)
    trims = {}
    for line in out.splitlines():
        if line.startswith('TRIM\t'):
            _, name, trim, spread = line.split('\t')
            trims[name] = (float(trim), float(spread))
    return trims

def num(v):
    s = ('%.2f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def emit_cinder(presets, trims):
    lines = ['        // Cinder props: model / modelB (0 Tape Preamp .. 9 Broadcast Limiter Amp), punishOn, steepOn, autoGainOn,',
             '        // msMode (0 stereo, 1 M/S, 2 mid only, 3 side only), oversample (0..4 = 1x..16x), linearPhaseOn. Output = measured trim.',
             '        { "Cinder", {']
    for p in presets:
        trim = max(-24.0, min(12.0, trims[p['name']][0]))
        vals = ['N ("drive", %s)' % num(p['drive']), 'N ("blend", %s)' % num(p['blend']), 'N ("bias", %s)' % num(p['bias']),
                'N ("dynamics", %s)' % num(p['dynamics']), 'N ("lowCut", %s)' % num(p['lowCut']), 'N ("thump", %s)' % num(p['thump']),
                'N ("tone", %s)' % num(p['tone']), 'N ("highCut", %s)' % num(p['highCut']), 'N ("output", %s)' % num(trim), 'N ("mix", %s)' % num(p['mix'])]
        props = ['{ "model", %d }' % p['model'], '{ "modelB", %d }' % p['modelB'], '{ "punishOn", %d }' % int(p['punish']),
                 '{ "steepOn", %d }' % int(p['steep']), '{ "autoGainOn", %d }' % int(p['autoGain']), '{ "msMode", %d }' % p['ms'],
                 '{ "oversample", %d }' % p['oversample'], '{ "linearPhaseOn", %d }' % int(p['linear'])]
        lines.append('            %s (%s, { %s },\n                { %s }),' % ('A' if p['adv'] else 'B', json.dumps(p['name']), ', '.join(vals), ', '.join(props)))
    lines.append('        } },')
    return '\n'.join(lines)

def main():
    names = [p['name'] for p in CINDER]
    assert len(names) == len(set(names)), 'duplicate preset names'
    cats = sorted(set(n.split(' | ')[0] for n in names))
    print('Cinder: %d presets in %d categories: %s' % (len(CINDER), len(cats), ', '.join(cats)))
    write_table(os.path.join(ROOT, 'tests', 'presets111_table.h'), CINDER)
    trims = calibrate()
    missing = [n for n in names if n not in trims]
    assert not missing, missing
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111.py - factory presets for the 1.11 Color & Space / Voice plug-ins. Do not edit by hand.\n')
        f.write('// Included inside SignaturePresets::bank() (SignaturePresets.h), after SignaturePresets181fx.inl.\n')
        f.write(emit_cinder(CINDER, trims) + '\n')
    worst = max(trims.values(), key=lambda t: t[1])
    print('wrote', out, '- widest spread between the three test signals: %.2f dB' % worst[1])

if __name__ == '__main__':
    main()
