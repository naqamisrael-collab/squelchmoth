#!/usr/bin/env python3
"""Factory presets for Moth Tune FX 2 (140). Writes Source/SignaturePresets111j.inl.

Each preset's Output trim comes from scripts/e4_audit_corrections.json: the level the app's own preset audit
(Squelchmoth --preset-audit) measured on its sung vocal (+-1 dB at unity), peak under +5.5 dBFS.
Modules: 1 Mutate, 2 Duet, 3 Vocode, 4 Tube, 5 Breath, 6 Reverb, 7 Echo, 8 Chorus, 9 Compress, 10 Low-pass, 11 High-pass,
12 Tilt EQ, 13 Octaver, 14 Bit-crush, 15 Talkbox, 16 Stutter, 17 EQ, 18 Phaser, 19 Flanger, 20 Auto-Pan, 21 Fuzz.
Triggers: 0 Always, 1 Auto, 2 Momentary, 3 Toggle. Motion modes: 0 off, 1 auto, 2 momentary, 3 toggle, 4 always.
Motion styles: 0 Hard, 1 Soft, 2 Legato. Patterns (1-based): 1 Up Arp, 2 Down Arp, 3 Up & Down, 4 Trill, 5 Octave Bounce,
6 Stutter Fifths, 7 Triplet Run, 8 Trap Flip, 9 Yodel, 10 Chromatic Rise, 11 Robot Steps, 12 Phone Ring, 13 Pentatonic Run,
14 Minor Drop, 15 Fifth Pulse, 16 Siren, 17 Gate Steps, 18 Sparkle, 19 Half-Step Wobble, 20 Wide Leap, 21 Bounce Ball,
22 Glitch Run, 23 Slow Climb, 24 Call Response.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MUT, DUET, VOC, TUBE, BREATH, VERB, ECHO, CHOR, COMP, LP, HP, TILT, OCT, CRUSH, TALK, STUT, EQ, PHASE, FLANGE, PAN, FUZZ = range(1, 22)

def fx(name, retune=0, slots=(), pattern=0, mode=0, style=0, glide=None, rate=None, engine=0, **kw):
    vals = {'retune': retune}
    props = {'engine': engine, 'motionStyle': style}
    for k in ('humanize', 'formant', 'throat', 'transpose', 'mix'):
        if k in kw: vals[k] = kw.pop(k)
    if glide is not None: vals['motionGlide'] = glide
    if rate is not None: vals['motionRate'] = rate
    for i, sl in enumerate(slots):
        t, x, y, mix = sl[:4]; trig = sl[4] if len(sl) > 4 else 0
        n = i + 1
        props['slotType%d' % n] = t
        if trig: props['slotTrig%d' % n] = trig
        vals['s%dx' % n] = x; vals['s%dy' % n] = y
        if mix != 100: vals['s%dmix' % n] = mix
    if pattern: props['motionPattern'] = pattern
    if mode: props['motionMode'] = mode
    props.update(kw)
    return dict(name=name, vals=vals, props=props)

P = []
def cat(c, items):
    for it in items: it['name'] = c + ' | ' + it['name']; P.append(it)

cat('Hard Tune', [
    fx('Clean Hard Tune'),
    fx('Hard Tune Polish', slots=[(EQ, .72, .6, 100), (COMP, .55, .3, 70)]),
    fx('Hard Tune Air', slots=[(TILT, .55, .55, 100)]),
    fx('Hard Tune Room', slots=[(VERB, .35, .55, 20)]),
    fx('Hard Tune Slap', slots=[(ECHO, .15, .2, 25)]),
    fx('Hard Tune Glued', slots=[(COMP, .5, .4, 100)], humanize=10),
    fx('Snappy Pop Tune', retune=5, slots=[(EQ, .8, .62, 100)]),
    fx('Fast Natural Tune', retune=25, humanize=30),
    fx('Hard Tune Studio', retune=0, slots=[(COMP, .55, .3, 60), (VERB, .4, .6, 15)], engine=1),
    fx('Hard Tune Bright Hall', slots=[(TILT, .6, .5, 100), (VERB, .7, .5, 25)]),
])
cat('Robots & Machines', [
    fx('Robot Voice', slots=[(MUT, .5, .15, 55), (TUBE, .35, .6, 100)]),
    fx('Metal Android', slots=[(MUT, .75, .2, 60), (EQ, .65, .7, 100)]),
    fx('Cyborg Phaser', slots=[(MUT, .5, .1, 40), (PHASE, .4, .6, 70)]),
    fx('Ring Machine', slots=[(MUT, .25, .5, 70)]),
    fx('Factory Unit', slots=[(MUT, .6, .2, 50), (CRUSH, .3, .1, 50), (HP, .25, .2, 100)]),
    fx('Circuit Bent', slots=[(CRUSH, .55, .3, 70), (MUT, .8, .6, 35)]),
    fx('Space Tyrant Ring', slots=[(MUT, .35, .05, 80), (LP, .7, .3, 100)]),
    fx('Synth Voice Box', slots=[(VOC, .5, .5, 70), (CHOR, .3, .4, 40)]),
    fx('Steel Throat', formant=-3, slots=[(MUT, .55, .2, 45), (FUZZ, .3, .5, 30)]),
    fx('Servo Steps', pattern=11, mode=1, slots=[(MUT, .5, .15, 40)]),
    fx('Mainframe', slots=[(VOC, .45, .3, 60), (CRUSH, .2, .05, 40)]),
    fx('Toy Robot', formant=5, slots=[(MUT, .7, .3, 50), (CRUSH, .4, .2, 40)]),
])
cat('Motion Patterns', [
    fx('Up Arp Motion', pattern=1, mode=1, glide=15, slots=[(ECHO, .4, .3, 30)]),
    fx('Down Arp Motion', pattern=2, mode=1, glide=15),
    fx('Up and Down Soft', pattern=3, mode=1, style=1),
    fx('Trill Hard', pattern=4, mode=1),
    fx('Octave Bounce Legato', pattern=5, mode=1, style=2),
    fx('Trap Flip Motion', pattern=8, mode=1, glide=10, slots=[(COMP, .5, .4, 70)]),
    fx('Yodel Steps', pattern=9, mode=1, style=1),
    fx('Chromatic Riser', pattern=10, mode=1),
    fx('Pentatonic Run Soft', pattern=13, mode=1, style=1),
    fx('Minor Drop Legato', pattern=14, mode=1, style=2),
    fx('Fifth Pulse', pattern=15, mode=1),
    fx('Siren Melody', pattern=16, mode=1, style=1, slots=[(VERB, .5, .5, 20)]),
    fx('Sparkle Steps', pattern=18, mode=1, slots=[(TILT, .6, .7, 100)]),
    fx('Wide Leap Legato', pattern=20, mode=1, style=2, slots=[(DUET, .4, .4, 40)]),
    fx('Bounce Ball', pattern=21, mode=1, style=1),
    fx('Call and Response', pattern=24, mode=1, slots=[(ECHO, .3, .35, 25)]),
])
cat('Momentary FX', [
    fx('Hold for Robot', slots=[(MUT, .5, .15, 70, 2)]),
    fx('Hold for Octave Down', slots=[(OCT, .8, .1, 100, 2)]),
    fx('Hold for Stutter', slots=[(STUT, .65, .9, 100, 2)]),
    fx('Hold for Echo Throw', slots=[(ECHO, .45, .55, 60, 2)]),
    fx('Hold for Big Verb', slots=[(VERB, .85, .4, 50, 2)]),
    fx('Hold for Telephone', slots=[(HP, .45, .2, 100, 2), (LP, .66, .25, 100, 2)]),
    fx('Hold for Arp', pattern=1, mode=2, style=1),
    fx('Toggle Vocoder', slots=[(VOC, .5, .6, 80, 3)]),
    fx('Toggle Flanger', slots=[(FLANGE, .3, .7, 70, 3)]),
    fx('Hold for Fuzz', slots=[(FUZZ, .6, .5, 70, 2)]),
])
cat('Doubles & Width', [
    fx('Doubled Lead', retune=15, slots=[(DUET, .45, .35, 100), (VERB, .45, .5, 22)]),
    fx('Tight Double', retune=10, slots=[(DUET, .25, .15, 70)]),
    fx('Wide Double', retune=10, slots=[(DUET, .6, .55, 90), (PAN, .2, .2, 40)]),
    fx('Chorused Lead', retune=10, slots=[(CHOR, .3, .35, 60)]),
    fx('Stereo Spread', retune=15, slots=[(DUET, .5, .4, 60), (CHOR, .2, .3, 40)]),
    fx('Double and Air', retune=10, slots=[(DUET, .4, .3, 60), (TILT, .55, .6, 100)]),
    fx('Thick Unison', retune=5, slots=[(DUET, .7, .2, 80), (CHOR, .4, .5, 40)]),
    fx('Slow Pan Lead', slots=[(PAN, .1, .5, 100)]),
    fx('Auto-Pan Eighths', slots=[(PAN, .65, .7, 100)]),
    fx('Wide Phase Lead', slots=[(PHASE, .2, .4, 50), (DUET, .4, .3, 40)]),
])
cat('Space', [
    fx('Vocal Plate Tune', slots=[(VERB, .5, .55, 30)]),
    fx('Big Hall Tune', slots=[(VERB, .9, .4, 40)]),
    fx('Quarter Echo', slots=[(ECHO, .6, .4, 35)]),
    fx('Dotted Echo', slots=[(ECHO, .45, .45, 35)]),
    fx('Ping-Pong Wash', slots=[(ECHO, .35, .7, 40), (VERB, .7, .5, 25)]),
    fx('Dreamy Chorus Hall', slots=[(CHOR, .25, .5, 50), (VERB, .8, .45, 35)]),
    fx('Shimmer Space', slots=[(OCT, .0, .5, 40), (VERB, .95, .3, 50)]),
    fx('Dark Echo Room', slots=[(ECHO, .5, .5, 35), (LP, .62, .2, 100)]),
    fx('Echo Only When Singing', slots=[(ECHO, .6, .55, 40, 1)]),
    fx('Distant Voice', slots=[(HP, .3, .1, 100), (VERB, .85, .6, 60)]),
])
cat('Lo-Fi & Radio', [
    fx('Telephone Talk', retune=10, slots=[(HP, .47, .2, 100), (LP, .66, .25, 100), (CRUSH, .45, .12, 70)]),
    fx('Lo-Fi Radio', retune=10, slots=[(CRUSH, .55, .25, 100), (HP, .42, .3, 100), (LP, .62, .3, 100), (COMP, .5, .5, 100)]),
    fx('Walkie Talkie', slots=[(HP, .5, .3, 100), (LP, .6, .4, 100), (FUZZ, .25, .3, 40)]),
    fx('Vintage AM', slots=[(HP, .4, .2, 100), (LP, .55, .3, 100), (TUBE, .5, .3, 100)]),
    fx('Bit Pop', slots=[(CRUSH, .6, .2, 80)]),
    fx('Cassette Tune', retune=15, slots=[(TUBE, .4, .35, 100), (LP, .7, .1, 100), (CHOR, .1, .2, 30)]),
    fx('Crushed Robot', slots=[(MUT, .5, .2, 50), (CRUSH, .7, .4, 70)]),
    fx('Old Gramophone', slots=[(HP, .55, .1, 100), (LP, .55, .2, 100), (CRUSH, .3, .05, 40)]),
    fx('Drive-Thru Speaker', slots=[(HP, .5, .4, 100), (FUZZ, .4, .4, 50), (LP, .58, .5, 100)]),
    fx('Lo-Fi Bedroom', retune=20, slots=[(LP, .75, .15, 100), (CRUSH, .3, .1, 30), (VERB, .3, .6, 20)]),
])
cat('Talkbox & Vocode', [
    fx('Talkbox Funk', slots=[(TALK, .3, .6, 100), (TALK, .3, .5, 100), (ECHO, .4, .3, 30)]),
    fx('Talkbox Ah', slots=[(TALK, .0, .5, 100)]),
    fx('Talkbox Ee', slots=[(TALK, .5, .5, 100)]),
    fx('Talkbox Oo', slots=[(TALK, .99, .5, 100)]),
    fx('Talkbox Wah Glide', pattern=3, mode=1, style=2, slots=[(TALK, .3, .7, 100)]),
    fx('Vocoder Choir', slots=[(VOC, .5, .85, 100), (CHOR, .35, .6, 60), (VERB, .7, .4, 30)]),
    fx('Vocoder Lead', slots=[(VOC, .5, .3, 90)]),
    fx('Vocoder Low', slots=[(VOC, .2, .4, 90)]),
    fx('Vocoder High', slots=[(VOC, .8, .4, 90)]),
    fx('Vocoder Phaser', slots=[(VOC, .5, .5, 80), (PHASE, .3, .5, 60)]),
    fx('Talking Synth Arp', pattern=1, mode=1, slots=[(VOC, .5, .5, 80)]),
    fx('Vocode on Demand', slots=[(VOC, .5, .6, 90, 2)]),
])
cat('Octaves & Monsters', [
    fx('Octave Monster', transpose=-12, slots=[(OCT, .8, .1, 100), (TUBE, .4, .4, 60)]),
    fx('Octave Down Double', slots=[(OCT, .7, .0, 100)]),
    fx('Octave Up Sparkle', slots=[(OCT, .0, .7, 100)]),
    fx('Both Octaves', slots=[(OCT, .6, .5, 100)]),
    fx('Giant', transpose=-12, formant=-4),
    fx('Chipmunk', transpose=12, formant=4),
    fx('Demon', transpose=-7, formant=-6, slots=[(FUZZ, .3, .4, 30), (VERB, .6, .3, 20)]),
    fx('Ogre Growl', formant=-6, slots=[(OCT, .9, .0, 100), (FUZZ, .4, .3, 40)]),
    fx('Alien Mutation', formant=5, slots=[(MUT, .8, .8, 70), (CHOR, .7, .8, 60), (VERB, .6, .4, 30)]),
    fx('Fifth Ghost', transpose=7, formant=2, slots=[(VERB, .7, .5, 30)]),
])
cat('Drive & Fuzz', [
    fx('Tube Warmth', slots=[(TUBE, .35, .5, 100)]),
    fx('Tube Crunch', slots=[(TUBE, .65, .45, 100)]),
    fx('Fuzz Vocal', slots=[(FUZZ, .45, .5, 70)]),
    fx('Fuzz Wall', slots=[(FUZZ, .8, .6, 90), (COMP, .5, .5, 100)]),
    fx('Gritty Lead', slots=[(TUBE, .5, .5, 70), (EQ, .7, .65, 100)]),
    fx('Distorted Megaphone', slots=[(HP, .45, .3, 100), (FUZZ, .6, .4, 80), (LP, .6, .4, 100)]),
    fx('Saturated Double', slots=[(TUBE, .45, .5, 100), (DUET, .4, .3, 60)]),
    fx('Fuzz Octave', slots=[(OCT, .7, .2, 100), (FUZZ, .5, .4, 60)]),
    fx('Dirty Phaser', slots=[(FUZZ, .4, .5, 50), (PHASE, .3, .6, 60)]),
    fx('Warm Compressed Drive', slots=[(TUBE, .4, .5, 100), (COMP, .55, .45, 100)]),
])
cat('Modulation', [
    fx('Slow Phaser', slots=[(PHASE, .15, .6, 60)]),
    fx('Fast Phaser', slots=[(PHASE, .7, .5, 60)]),
    fx('Jet Flanger', slots=[(FLANGE, .2, .8, 60)]),
    fx('Subtle Flanger', slots=[(FLANGE, .15, .3, 40)]),
    fx('Chorus Dream', slots=[(CHOR, .2, .6, 60)]),
    fx('Vibrato Chorus', slots=[(CHOR, .6, .4, 100)]),
    fx('Auto-Pan Quarter', slots=[(PAN, .5, .6, 100)]),
    fx('Auto-Pan Sixteenths', slots=[(PAN, .99, .5, 100)]),
    fx('Phase and Pan', slots=[(PHASE, .3, .5, 50), (PAN, .4, .5, 100)]),
    fx('Flanged Echo', slots=[(FLANGE, .25, .6, 50), (ECHO, .45, .4, 30)]),
    fx('Swirl Hall', slots=[(PHASE, .2, .7, 50), (VERB, .8, .5, 35)]),
    fx('Spaceship', slots=[(FLANGE, .5, .9, 70), (PHASE, .6, .7, 50)]),
])
cat('Stutter & Gates', [
    fx('Stutter Gate', slots=[(STUT, .6, .8, 100)]),
    fx('Quarter Chop', slots=[(STUT, .3, .9, 100)]),
    fx('Sixteenth Chop', slots=[(STUT, .99, .7, 100)]),
    fx('Gate Steps Motion', pattern=17, mode=1),
    fx('Glitch Run', pattern=22, mode=1, slots=[(CRUSH, .3, .1, 40)]),
    fx('Robot Steps Gate', pattern=11, mode=1, slots=[(MUT, .5, .2, 40)]),
    fx('Chopped Echo', slots=[(STUT, .6, .7, 100), (ECHO, .4, .5, 30)]),
    fx('Half-Step Wobble', pattern=19, mode=1, style=1),
])
cat('Character Voices', [
    fx('Male to Female', retune=25, transpose=12, formant=2.5, throat=88),
    fx('Female to Male', retune=25, transpose=-12, formant=-2.5, throat=115),
    fx('Breathy Air', retune=20, humanize=30, slots=[(BREATH, .5, .6, 100), (TILT, .62, .6, 100)]),
    fx('Whisper Ghost', slots=[(BREATH, .9, .7, 100), (HP, .35, .2, 100), (VERB, .7, .5, 30)]),
    fx('Cartoon Hero', transpose=5, formant=3, slots=[(COMP, .5, .5, 100)]),
    fx('Deep Narrator', retune=40, formant=-2, throat=110, slots=[(EQ, .2, .65, 100), (COMP, .5, .4, 100)]),
    fx('Tiny Fairy', transpose=12, formant=6, slots=[(CHOR, .4, .5, 50), (VERB, .7, .4, 30)]),
    fx('Gentle Giant', transpose=-5, formant=-3, slots=[(TUBE, .3, .4, 60)]),
    fx('Child Voice', transpose=7, formant=4, throat=85),
    fx('Ancient Spirit', transpose=-12, formant=-1, slots=[(CHOR, .2, .7, 50), (VERB, .95, .3, 60)]),
])

def num(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def main():
    names = [p['name'] for p in P]
    dup = set(n for n in names if names.count(n) > 1)
    assert not dup, dup
    print('Moth Tune FX 2: %d presets in %d categories' % (len(P), len(set(n.split(' | ')[0] for n in names))))
    assert len(P) == 140
    cpath = os.path.join(ROOT, 'scripts', 'e4_audit_corrections.json')
    corr = json.load(open(cpath)).get('Moth Tune FX 2', {}) if os.path.exists(cpath) else {}
    lines = []
    for p in P:
        trim = max(-24.0, min(12.0, corr.get(p['name'].replace(' | ', ' / '), 0.0)))
        vals = ', '.join('N (%s, %s)' % (json.dumps(k), num(v)) for k, v in p['vals'].items()) + ', N ("output", %s)' % num(round(trim, 2))
        prs = ', '.join('{ "%s", %d }' % (k, int(v)) for k, v in p['props'].items())
        lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111j.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111j.py - factory presets for Moth Tune FX 2. Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111i.inl; bank() calls add111j().\n\n')
        f.write('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111j_1()   // Moth Tune FX 2\n{\n    return {\n%s\n    };\n}\n' % '\n'.join(lines))
        f.write('inline void add111j (std::map<juce::String, std::vector<Preset>>& m)\n{\n    m["Moth Tune FX 2"] = presets111j_1();\n}\n')
    print('wrote', out, '- corrections for', len(corr))

if __name__ == '__main__':
    main()
