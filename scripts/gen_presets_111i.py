#!/usr/bin/env python3
"""Factory presets for the 1.11 Voice second wave, part 3: Vox Moth (140), Talk Wing (40), Clean Room (40).

Writes Source/SignaturePresets111i.inl. Each preset's Output trim comes from scripts/e3_audit_corrections.json: the level
the app's own preset audit (Squelchmoth --preset-audit) measured on its sung vocal, so every preset leaves the vocal at
the level it came in (+-1 dB), peak under +3 dBFS. Run, build, audit, write the corrections, run again, build.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------------------------------- Vox Moth
# The 20 voicings (as in Source/SqVoice111gDsp.h): bands, lo, hi, shape, resonance, attack, release, smear (0-1), unvoiced (0-1),
# noise (0-1), wave1, wave2, pw (0-1), osc2 dB, detune
VOICINGS = [
    ("Studio Ten", 10, 150, 6000, 0, 35, 4, 70, .10, .6, .30, 0, 1, .50, -8, 6),
    ("Sixteen Classic", 16, 100, 7500, 0, 40, 3, 50, .10, .6, .35, 0, 1, .50, -6, 7),
    ("Twenty Analog", 20, 90, 8500, 1, 45, 3, 45, .08, .6, .35, 0, 0, .50, -6, 9),
    ("String Machine", 8, 200, 5000, 3, 20, 8, 120, .25, .3, .15, 0, 0, .50, -3, 14),
    ("Robot Radio", 8, 300, 3400, 1, 60, 2, 35, .05, .4, .25, 1, 1, .35, -10, 3),
    ("Speech Lab", 12, 120, 6500, 1, 55, 2, 30, .05, .8, .50, 0, 1, .50, -9, 4),
    ("Polyphonic 32", 32, 80, 10000, 0, 45, 2, 40, .10, .6, .30, 0, 0, .50, -6, 8),
    ("Digital 64", 64, 60, 14000, 1, 60, 1, 25, .04, .7, .30, 0, 0, .50, -6, 5),
    ("Space Choir", 24, 120, 9000, 2, 70, 6, 160, .30, .5, .20, 0, 0, .50, -4, 18),
    ("Funk Box", 14, 140, 6000, 2, 65, 1, 25, .05, .5, .30, 1, 0, .30, -8, 5),
    ("Phone Line", 6, 350, 3300, 1, 50, 3, 40, .05, .5, .30, 1, 1, .50, -12, 0),
    ("Glass Array", 40, 150, 12000, 2, 85, 2, 90, .12, .4, .20, 1, 1, .20, -6, 10),
    ("Wide Pad", 48, 80, 12000, 3, 25, 8, 200, .35, .4, .15, 0, 0, .50, -3, 16),
    ("Sibilant Twenty", 20, 100, 11000, 0, 40, 2, 40, .08, 1.0, .60, 0, 1, .50, -6, 6),
    ("Chip Four", 4, 200, 4000, 1, 40, 1, 20, .00, .2, .10, 1, 1, .25, -60, 0),
    ("Warm Eighteen", 18, 90, 6500, 0, 30, 5, 80, .18, .5, .25, 0, 0, .50, -6, 11),
    ("Formant Ten", 10, 250, 4500, 2, 75, 3, 60, .06, .5, .25, 0, 1, .45, -8, 5),
    ("Bright Twenty-Four", 24, 150, 13000, 1, 50, 2, 35, .06, .7, .35, 0, 1, .50, -6, 7),
    ("Dark Hall Twelve", 12, 70, 4500, 0, 35, 6, 140, .20, .3, .15, 0, 0, .50, -4, 12),
    ("Hybrid Forty", 40, 100, 11000, 0, 50, 2, 45, .10, .6, .30, 0, 1, .50, -6, 8),
]
VI = {v[0]: i for i, v in enumerate(VOICINGS)}
VM_PARAMS = ['bands', 'loHz', 'hiHz', 'resonance', 'stretch', 'slide', 'bandShift', 'attack', 'release', 'smear', 'tilt', 'unvoiced', 'noise', 'pw',
             'osc2', 'interval2', 'detune', 'glide', 'tune', 'retune', 'ring', 'ringHz', 'drive', 'chorus', 'width', 'xyX', 'xyY', 'mix', 'voices'] + ['lv%d' % i for i in range(1, 17)]
CURVES = {
    'flat': [0] * 16,
    'smile': [3, 2, 1, 0, -1, -2, -2, -2, -2, -1, 0, 1, 2, 3, 3, 3],
    'presence': [-3, -2, -1, 0, 0, 1, 2, 4, 5, 4, 2, 0, -1, -2, -3, -4],
    'warm': [3, 3, 2, 2, 1, 1, 0, 0, -1, -2, -3, -4, -5, -6, -6, -6],
    'air': [-2, -2, -1, -1, 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 6],
    'phone': [-18, -14, -8, -2, 2, 4, 5, 5, 4, 2, -2, -8, -14, -18, -20, -20],
    'vowel': [0, 2, 5, 6, 3, 0, -2, 2, 5, 4, 0, -3, -2, 0, 1, 0],
    'scoop': [2, 2, 1, -2, -5, -7, -7, -5, -2, 0, 2, 3, 3, 2, 1, 0],
    'comb': [4, -4, 4, -4, 4, -4, 4, -4, 4, -4, 4, -4, 4, -4, 4, -4],
    'peak2k': [-4, -4, -3, -2, -1, 0, 2, 5, 7, 5, 2, 0, -2, -3, -4, -5],
    'dark': [2, 2, 2, 1, 1, 0, -1, -3, -5, -7, -9, -11, -13, -15, -16, -16],
}

def vm(name, voicing, carrier=0, curve='flat', **kw):
    v = VOICINGS[VI[voicing]]
    vals = dict(bands=v[1], loHz=v[2], hiHz=v[3], resonance=v[5], attack=v[6], release=v[7], smear=round(v[8] * 100), unvoiced=round(v[9] * 100),
                noise=round(v[10] * 100), pw=round(v[13] * 100), osc2=v[14], detune=v[15])
    props = dict(carrier=carrier, shape=v[4], wave1=v[11], wave2=v[12], octave2=2, driveVoicing=0, voicing=VI[voicing], autoKeyOn=1, keyRoot=0, scale=1, voiceType=1)
    for k in list(kw):
        if k in props: props[k] = kw.pop(k)
    vals.update(kw)
    for i, x in enumerate(CURVES[curve]):
        if x: vals['lv%d' % (i + 1)] = x
    return dict(name=name, vals=vals, props=props)

VOX = []
def cat(c, items):
    for it in items:
        it['name'] = c + ' | ' + it['name']
        VOX.append(it)

cat('Classic Robots', [
    vm('Sixteen Band Robot', 'Sixteen Classic'),
    vm('Ten Band Studio', 'Studio Ten'),
    vm('Twenty Steep Robot', 'Twenty Analog'),
    vm('Monotone Machine', 'Sixteen Classic', tune=100, retune=0, glide=0, voices=1),
    vm('Factory Announcer', 'Speech Lab', curve='presence'),
    vm('Tin Can Android', 'Robot Radio', resonance=70),
    vm('Steel Throat', 'Twenty Analog', resonance=65, curve='peak2k'),
    vm('Chrome Speaker', 'Bright Twenty-Four'),
    vm('Assembly Line', 'Sixteen Classic', attack=1, release=30),
    vm('Old Laboratory', 'Speech Lab', release=60, smear=15),
    vm('Pulse Robot', 'Sixteen Classic', wave1=1, wave2=1, pw=35),
    vm('Saw Robot', 'Sixteen Classic', wave1=0, wave2=0),
])
cat('Vocoder Leads', [
    vm('Bright Talking Lead', 'Bright Twenty-Four', curve='presence', chorus=20),
    vm('Fat Saw Lead', 'Twenty Analog', osc2=-3, detune=14, chorus=25),
    vm('Octave Lead', 'Sixteen Classic', octave2=3, osc2=-6),
    vm('Fifth Stack Lead', 'Sixteen Classic', interval2=7, osc2=-5),
    vm('Glide Lead', 'Twenty Analog', glide=180, voices=1),
    vm('Square Lead', 'Formant Ten', wave1=1, wave2=1, pw=50),
    vm('Driven Lead', 'Hybrid Forty', driveVoicing=2, drive=45),
    vm('Crunch Talk Lead', 'Sixteen Classic', driveVoicing=5, drive=40),
    vm('Silky Lead', 'Warm Eighteen', chorus=35, curve='warm'),
    vm('Wide Stereo Lead', 'Polyphonic 32', width=180, chorus=30),
    vm('Narrow Pulse Lead', 'Formant Ten', wave1=1, pw=15),
    vm('Detuned Lead', 'Twenty Analog', detune=30, chorus=20),
    vm('Formant Lead', 'Formant Ten', resonance=80, curve='vowel'),
    vm('Air Lead', 'Bright Twenty-Four', curve='air', unvoiced=80),
])
cat('Pads & Choirs', [
    vm('Space Choir', 'Space Choir', chorus=40, width=170),
    vm('Breathing Pad', 'Wide Pad', chorus=45, width=180),
    vm('String Voices', 'String Machine', chorus=50),
    vm('Cathedral Robots', 'Space Choir', release=300, smear=40),
    vm('Velvet Choir', 'Warm Eighteen', release=180, smear=30, chorus=40, curve='warm'),
    vm('Glass Choir', 'Glass Array', release=200, chorus=30),
    vm('Slow Bloom Pad', 'Wide Pad', attack=40, release=400),
    vm('Angel Stack', 'Space Choir', octave2=3, osc2=-4),
    vm('Dark Choir', 'Dark Hall Twelve', chorus=35, curve='dark'),
    vm('Ensemble Wash', 'Polyphonic 32', smear=50, release=250, chorus=55, width=200),
    vm('Soft Pulse Pad', 'Wide Pad', wave1=1, wave2=1, pw=30, chorus=40),
    vm('Frozen Vowels', 'Space Choir', attack=60, release=600, smear=60),
    vm('Wide Unison Pad', 'String Machine', detune=40, chorus=50, width=200),
    vm('Halo Pad', 'Wide Pad', curve='air', chorus=40),
])
cat('Voice Track', [
    vm('Tuned Robot', 'Sixteen Classic', carrier=1, tune=100, retune=0),
    vm('Natural Tuned Robot', 'Twenty Analog', carrier=1, tune=100, retune=40),
    vm('Loose Tracker', 'Sixteen Classic', carrier=1, tune=40, retune=80),
    vm('Octave Down Tracker', 'Warm Eighteen', carrier=1, octave2=1, osc2=-3),
    vm('Fifth Harmony Tracker', 'Sixteen Classic', carrier=1, interval2=7, osc2=-4),
    vm('Third Harmony Tracker', 'Twenty Analog', carrier=1, interval2=4, osc2=-5),
    vm('Hard Snap Robot', 'Funk Box', carrier=1, tune=100, retune=0, glide=0),
    vm('Minor Key Tracker', 'Sixteen Classic', carrier=1, scale=2, keyRoot=9),
    vm('Smooth Glide Tracker', 'Polyphonic 32', carrier=1, retune=120, glide=90),
    vm('Talking Synth Tracker', 'Formant Ten', carrier=1, curve='vowel'),
    vm('Raw Pitch Follower', 'Hybrid Forty', carrier=1, tune=0),
    vm('Low Voice Tracker', 'Dark Hall Twelve', carrier=1, voiceType=3),
])
cat('Chords (MIDI)', [
    vm('Classic Chord Vocoder', 'Polyphonic 32', chorus=25),
    vm('Rich Chord Stack', 'Hybrid Forty', osc2=-4, detune=12, chorus=30),
    vm('Eight Voice Choir', 'Polyphonic 32', voices=8, smear=20, chorus=35),
    vm('Four Voice Chords', 'Sixteen Classic', voices=4),
    vm('Organ Chords', 'Warm Eighteen', wave1=1, wave2=1, pw=50, octave2=3, osc2=-8),
    vm('Detailed Chords', 'Digital 64', chorus=20),
    vm('Soft Chord Bed', 'Wide Pad', voices=8, chorus=40),
    vm('Pulse Chords', 'Twenty Analog', wave1=1, wave2=1, pw=30),
    vm('Bright Chords', 'Bright Twenty-Four', curve='air'),
    vm('Warm Chords', 'Warm Eighteen', curve='warm', chorus=30),
    vm('Gospel Chords', 'Hybrid Forty', smear=20, release=90, chorus=30),
    vm('Wide Chords', 'Polyphonic 32', width=200, chorus=45),
])
cat('Speech & Radio', [
    vm('Transmission', 'Robot Radio', curve='phone'),
    vm('Phone Robot', 'Phone Line'),
    vm('Station Announcer', 'Speech Lab', curve='presence'),
    vm('Walkie Robot', 'Robot Radio', resonance=75, drive=30, driveVoicing=4),
    vm('Clear Speech Coder', 'Speech Lab', bands=20, unvoiced=90),
    vm('Narrow Band Speech', 'Phone Line', bands=8),
    vm('Mission Control', 'Robot Radio', chorus=15),
    vm('Spoken Word Robot', 'Speech Lab', release=45),
    vm('Intercom', 'Phone Line', resonance=70, curve='peak2k'),
    vm('Numbers Station', 'Speech Lab', ring=20, ringHz=600),
])
cat('Synth Bass Talk', [
    vm('Talking Bass', 'Funk Box', octave2=1, osc2=-2, loHz=60, hiHz=4000),
    vm('Sub Talk', 'Dark Hall Twelve', octave2=0, osc2=-3, release=60),
    vm('Wah Bass', 'Formant Ten', resonance=85, octave2=1),
    vm('Funk Bass Talker', 'Funk Box', attack=1, release=20, octave2=1),
    vm('Growl Bass', 'Funk Box', driveVoicing=3, drive=50, octave2=1),
    vm('Rubber Bass', 'Twenty Analog', glide=60, octave2=1, voices=1),
    vm('Pulse Bass Talk', 'Funk Box', wave1=1, wave2=1, pw=25, octave2=1),
    vm('Deep Speaker', 'Dark Hall Twelve', curve='warm'),
])
cat('Bright & Modern', [
    vm('Crystal Clear', 'Digital 64', curve='air'),
    vm('Sixty-Four Detail', 'Digital 64'),
    vm('Pop Sheen Vocoder', 'Bright Twenty-Four', chorus=25, curve='presence'),
    vm('Airy Sibilance', 'Sibilant Twenty', curve='air'),
    vm('Modern Forty', 'Hybrid Forty'),
    vm('Hi-Fi Robot', 'Digital 64', resonance=45, release=30),
    vm('Silver Voice', 'Bright Twenty-Four', resonance=60, chorus=20),
    vm('Neon Talker', 'Bright Twenty-Four', wave1=1, pw=40, chorus=30),
    vm('Sharp Consonants', 'Sibilant Twenty', unvoiced=100, noise=70),
    vm('Smooth Modern', 'Hybrid Forty', smear=25, chorus=20),
    vm('Stereo Detail', 'Digital 64', width=190),
    vm('Fast Tracker', 'Digital 64', attack=0.5, release=15),
])
cat('Dark & Cinematic', [
    vm('Dark Hall Voice', 'Dark Hall Twelve'),
    vm('Titan Voice', 'Dark Hall Twelve', octave2=1, osc2=-2, curve='warm'),
    vm('Underworld', 'Dark Hall Twelve', slide=-5, curve='dark'),
    vm('Monolith', 'Warm Eighteen', slide=-3, release=200),
    vm('Shadow Choir', 'Space Choir', slide=-4, curve='dark'),
    vm('Deep Space Signal', 'Space Choir', ring=15, ringHz=90),
    vm('Cold Machine', 'Twenty Analog', curve='scoop'),
    vm('Low Prophet', 'Dark Hall Twelve', stretch=0.8),
    vm('Dread Pad', 'Wide Pad', slide=-6, curve='dark', chorus=30),
    vm('Iron Giant Voice', 'Funk Box', slide=-7, driveVoicing=2, drive=35),
    vm('Abyss', 'Dark Hall Twelve', octave2=0, osc2=0, slide=-8),
    vm('War Drum Voice', 'Sixteen Classic', curve='warm', driveVoicing=1, drive=40),
])
cat('Glass & Ring', [
    vm('Glass Array', 'Glass Array'),
    vm('Ringing Bells', 'Glass Array', ring=40, ringHz=880),
    vm('Metal Mouth', 'Sixteen Classic', ring=35, ringHz=300),
    vm('Crystal Resonator', 'Glass Array', resonance=95, release=150),
    vm('Ring Talker', 'Twenty Analog', ring=50, ringHz=150),
    vm('Wind Chime Voice', 'Glass Array', smear=20, chorus=40),
    vm('Ring Choir', 'Space Choir', ring=30, ringHz=440),
    vm('Bell Lead', 'Glass Array', octave2=3, osc2=-6),
    vm('Mirror Voice', 'Glass Array', stretch=1.4),
    vm('Chime Robot', 'Formant Ten', resonance=95),
])
cat('Formant Play', [
    vm('Small Robot', 'Sixteen Classic', slide=6),
    vm('Tiny Robot', 'Sixteen Classic', slide=12),
    vm('Big Robot', 'Sixteen Classic', slide=-6),
    vm('Giant Robot', 'Sixteen Classic', slide=-10),
    vm('Stretched Mouth', 'Twenty Analog', stretch=1.6),
    vm('Squeezed Mouth', 'Twenty Analog', stretch=0.6),
    vm('Band Shift Up', 'Sixteen Classic', bandShift=2),
    vm('Band Shift Down', 'Sixteen Classic', bandShift=-2),
    vm('Inside Out', 'Twenty Analog', stretch=-1.0 if False else 0.4, slide=4),
    vm('X/Y Sweep Start', 'Twenty Analog', xyX=30, xyY=70),
    vm('Vowel Morph', 'Formant Ten', curve='vowel', xyX=60),
    vm('Comb Voice', 'Hybrid Forty', curve='comb'),
])
cat('Key Input', [
    vm('Key Input Classic', 'Sixteen Classic', carrier=3),
    vm('Key Input Detail', 'Digital 64', carrier=3),
    vm('Key Input Drums', 'Funk Box', carrier=3, attack=1, release=20),
    vm('Key Input Pad', 'Wide Pad', carrier=3, chorus=30),
    vm('Key Input Strings', 'String Machine', carrier=3),
    vm('Key Input Smooth', 'Hybrid Forty', carrier=3, smear=25),
])
cat('Lo-Fi Talk', [
    vm('Game Console', 'Chip Four'),
    vm('Eight Band Toy', 'Robot Radio', bands=8, drive=40, driveVoicing=6),
    vm('Cassette Robot', 'Warm Eighteen', drive=45, driveVoicing=2, curve='dark'),
    vm('Broken Speaker', 'Phone Line', drive=60, driveVoicing=6),
    vm('Pocket Calculator', 'Chip Four', wave1=1, pw=50),
    vm('Crushed Choir', 'Space Choir', drive=55, driveVoicing=5),
])

# ---------------------------------------------------------------------------------------------------- Talk Wing
TW_PARAMS = ['mouth', 'formant', 'precision', 'articulation', 'follow', 'sibilance', 'gate', 'tube', 'brightness', 'buzzTone', 'glide', 'mix']
def tw(name, source=0, engine=1, buzzOctave=2, voiceType=1, **kw):
    return dict(name=name, vals=kw, props=dict(source=source, engine=engine, voiceType=voiceType, buzzOctave=buzzOctave))
TALK = []
def tcat(c, items):
    for it in items: it['name'] = c + ' | ' + it['name']; TALK.append(it)
tcat('Guitar Talk', [
    tw('Classic Guitar Talk', tube=50),
    tw('Rock Talk Box', tube=60, brightness=1.5, articulation=20),
    tw('Smooth Guitar Talk', tube=40, articulation=50),
    tw('Sharp Consonant Guitar', sibilance=85, precision=70),
    tw('Funky Wah Talk', articulation=10, precision=80, tube=55),
    tw('Dark Tube Guitar', tube=85, brightness=-2),
    tw('Bright Guitar Speak', tube=30, brightness=3),
    tw('Slow Mouth Guitar', articulation=70, tube=50),
])
tcat('Synth Talk', [
    tw('Synth Talk Box', tube=45),
    tw('Talking Lead Synth', precision=70, tube=40, brightness=1),
    tw('Robot Talk Synth', precision=90, articulation=10, tube=20),
    tw('Soft Synth Speak', articulation=60, tube=55, sibilance=40),
    tw('Big Mouth Synth', formant=-3, tube=50),
    tw('Small Mouth Synth', formant=4, tube=35),
    tw('Crisp Synth Talk', sibilance=90, brightness=2),
    tw('Live Synth Talk', engine=0, tube=45),
])
tcat('Keys & Organ', [
    tw('Talking Organ', tube=40, formant=-1),
    tw('Electric Piano Talk', tube=35, articulation=40),
    tw('Funk Keys Mouth', precision=80, articulation=15, tube=30),
    tw('Church Organ Speaks', tube=60, articulation=50, formant=-2),
    tw('Keys Whisper', mouth=80, sibilance=80, tube=30),
    tw('Toy Piano Talk', formant=5, tube=25),
])
tcat('Bass Talk', [
    tw('Talking Bass', formant=-2, tube=50, brightness=1),
    tw('Synth Bass Mouth', formant=-1, precision=70, tube=45),
    tw('Growling Bass Talk', formant=-4, tube=70),
    tw('Funk Bass Speak', articulation=15, precision=75),
    tw('Sub Mouth', formant=-5, tube=60, brightness=-1.5),
    tw('Rubber Bass Talk', articulation=45, formant=-2),
])
tcat('Built-in Buzz', [
    tw('Buzz Talker', source=2, buzzTone=50),
    tw('Octave Down Buzz', source=2, buzzOctave=1, buzzTone=40),
    tw('Reedy Buzz', source=2, buzzTone=85, tube=40),
    tw('Smooth Buzz', source=2, buzzTone=15, glide=60),
    tw('Deep Buzz Voice', source=2, buzzOctave=0, tube=55),
    tw('Robot Buzz', source=2, buzzTone=60, glide=0, precision=80),
])
tcat('Character', [
    tw('Voice on Track, Key the Instrument', source=1, tube=45),
    tw('Formant Up Talk', formant=6, tube=30),
    tw('Formant Down Talk', formant=-6, tube=50),
    tw('Half Talk Blend', mix=60, tube=40),
    tw('Gated Talk', gate=-45, articulation=10),
    tw('Whisper Box', sibilance=100, mouth=90, tube=20, brightness=2),
])

# ---------------------------------------------------------------------------------------------------- Clean Room
CR_PARAMS = ['clean', 'hum', 'buzz', 'hiss', 'noise', 'room', 'clicks', 'roomDecay', 'guard', 'mix']
def cr(name, engine=1, humMode=0, **kw):
    return dict(name=name, vals=kw, props=dict(engine=engine, humMode=humMode))
CLEAN = []
def ccat(c, items):
    for it in items: it['name'] = c + ' | ' + it['name']; CLEAN.append(it)
ccat('One Knob', [
    cr('Light Clean', clean=20), cr('Gentle Clean', clean=35), cr('Standard Clean', clean=50), cr('Firm Clean', clean=65),
    cr('Deep Clean', clean=80, guard=60), cr('Maximum Clean', clean=100, guard=70),
])
ccat('Hum & Buzz', [
    cr('Hum Only Auto', clean=50, hum=100, buzz=0, hiss=0, noise=0, room=0, clicks=0),
    cr('Hum 50 Hz', clean=50, humMode=1, hum=100, buzz=40, hiss=0, noise=0, room=0, clicks=0),
    cr('Hum 60 Hz', clean=50, humMode=2, hum=100, buzz=40, hiss=0, noise=0, room=0, clicks=0),
    cr('Ground Loop Buzz', clean=60, hum=100, buzz=100, hiss=0, noise=10, room=0, clicks=0),
    cr('Guitar Amp Buzz', clean=55, hum=90, buzz=90, hiss=30, noise=20, room=0, clicks=0),
    cr('Lighting Buzz', clean=60, hum=60, buzz=100, hiss=20, noise=10, room=0, clicks=0),
])
ccat('Noise & Hiss', [
    cr('Hiss Removal', clean=50, hum=0, buzz=0, hiss=100, noise=30, room=0, clicks=0),
    cr('Preamp Hiss', clean=60, hum=20, buzz=0, hiss=90, noise=40, room=0),
    cr('Air Conditioner', clean=60, hum=40, buzz=20, hiss=40, noise=100, room=0),
    cr('Computer Fan', clean=55, hum=30, buzz=30, hiss=50, noise=90, room=0),
    cr('Street Noise', clean=70, hum=0, buzz=0, hiss=30, noise=100, room=10, guard=60),
    cr('Tape Hiss', clean=50, hum=0, buzz=0, hiss=100, noise=50, room=0, clicks=30),
    cr('Broadband Gentle', clean=40, hum=0, buzz=0, hiss=40, noise=80, room=0, guard=70),
    cr('Broadband Strong', clean=75, hum=0, buzz=0, hiss=60, noise=100, room=0, guard=55),
])
ccat('Rooms', [
    cr('Small Room', clean=50, room=80, roomDecay=0.4, noise=30),
    cr('Bedroom Booth', clean=55, room=90, roomDecay=0.5, noise=40),
    cr('Living Room', clean=60, room=90, roomDecay=0.8),
    cr('Bathroom Tiles', clean=70, room=100, roomDecay=1.2, guard=60),
    cr('Hall Recording', clean=70, room=100, roomDecay=1.8, guard=65),
    cr('Kitchen Echo', clean=60, room=90, roomDecay=0.7, hiss=20),
    cr('Untreated Studio', clean=50, room=70, roomDecay=0.6, noise=50),
])
ccat('Clicks & Mouth', [
    cr('Mouth Clicks', clean=50, clicks=100, hum=0, buzz=0, hiss=0, noise=10, room=0),
    cr('Lip Smacks', clean=60, clicks=100, hum=0, buzz=0, hiss=10, noise=10, room=0),
    cr('Vinyl Crackle', clean=60, clicks=100, hiss=60, noise=30, room=0),
    cr('Digital Clicks', clean=55, clicks=90, noise=10, room=0),
    cr('Cable Crackle', clean=55, clicks=80, hum=60, buzz=40),
])
ccat('Podcast & Dialogue', [
    cr('Podcast Voice', clean=55, room=60, roomDecay=0.5),
    cr('Interview On Location', clean=70, noise=100, room=50, guard=60),
    cr('Zoom Call Rescue', clean=75, noise=90, room=70, roomDecay=0.6, guard=60),
    cr('Voiceover Polish', clean=40, guard=70),
    cr('Phone Recording', clean=60, hiss=80, noise=70, clicks=60),
    cr('Live Stream Clean', engine=0, clean=50),
    cr('Lecture Hall', clean=70, room=100, roomDecay=1.5, noise=60),
    cr('Low Latency Monitor', engine=0, clean=35, guard=70),
])

PLUGS = {'Vox Moth': (VOX, 140), 'Talk Wing': (TALK, 40), 'Clean Room': (CLEAN, 40)}

def num(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s + 'f' if '.' in s else s

def main():
    cpath = os.path.join(ROOT, 'scripts', 'e3_audit_corrections.json')
    corr = json.load(open(cpath)) if os.path.exists(cpath) else {}
    fns, adds = [], []
    for k, (nm, (lst, want)) in enumerate(PLUGS.items()):
        names = [p['name'] for p in lst]
        dup = set(n for n in names if names.count(n) > 1)
        assert not dup, (nm, dup)
        print('%s: %d presets (want %d) in %d categories' % (nm, len(lst), want, len(set(n.split(' | ')[0] for n in names))))
        assert len(lst) == want, nm
        lines = []
        for p in lst:
            trim = max(-24.0, min(24.0, corr.get(nm, {}).get(p['name'].replace(' | ', ' / '), 0.0)))
            vals = ', '.join('N (%s, %s)' % (json.dumps(kk), num(vv)) for kk, vv in p['vals'].items()) + ', N ("output", %s)' % num(round(trim, 2))
            prs = ', '.join('{ "%s", %d }' % (kk, int(vv)) for kk, vv in p['props'].items())
            lines.append('        B (%s, { %s },\n            { %s }),' % (json.dumps(p['name']), vals, prs))
        fns.append('SQ_PRESETS_NOINLINE inline std::vector<Preset> presets111i_%d()   // %s\n{\n    return {\n%s\n    };\n}\n' % (k + 1, nm, '\n'.join(lines)))
        adds.append('    m[%s] = presets111i_%d();\n' % (json.dumps(nm), k + 1))
    out = os.path.join(ROOT, 'Source', 'SignaturePresets111i.inl')
    with open(out, 'w') as f:
        f.write('// Generated by scripts/gen_presets_111i.py - factory presets for the 1.11 Voice second wave (part 3). Do not edit by hand.\n')
        f.write('// Included in namespace SignaturePresets (SignaturePresets.h) after SignaturePresets111h.inl; bank() calls add111i().\n\n')
        f.write('\n'.join(fns))
        f.write('inline void add111i (std::map<juce::String, std::vector<Preset>>& m)\n{\n' + ''.join(adds) + '}\n')
    print('wrote', out, '- corrections for', sum(len(v) for v in corr.values()), 'presets')

if __name__ == '__main__':
    main()
