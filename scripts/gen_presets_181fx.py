#!/usr/bin/env python3
"""Factory presets for the 1.8.1 effects (Time Weaver, Larva Filter, Apex Triad).
Writes Source/SignaturePresets181fx.inl, included inside SignaturePresets::bank() after SignaturePresets181.inl."""
import os

OUT = os.path.join(os.path.dirname(__file__), '..', 'Source', 'SignaturePresets181fx.inl')

def fmt(v):
    if isinstance(v, int) or float(v).is_integer():
        return str(int(v))
    return repr(round(float(v), 4)) + 'f'

def preset(adv, name, vals):
    body = ', '.join('N ("%s", %s)' % (k, fmt(v)) for k, v in vals.items())
    return '            %s ("%s", { %s })' % ('A' if adv else 'B', name, body)

banks = {}

# ---------------------------------------------------------------- Time Weaver
TS = ['USER', 'Live', 'Half Time', 'Half Time Bar', 'Half Time 1/2 Bar', 'Quarter Time', 'Stutter 1/16', 'Stutter 1/8', 'Stutter 1/4',
      'Stutter 1/2', 'Repeat Bar', 'Repeat Beat 2', 'Repeat Last 1/8', 'Reverse Beat', 'Reverse Bar', 'Reverse 1/2 Bar', 'Scratch',
      'Tape Stop Beat 4', 'Tape Stop Bar', 'Build 1/16 > 1/32', 'Triplet Stutter', 'Off-beat Echo', 'Dotted Echo', 'Half Time + Stutter',
      'Double Time', 'Skip Beat', 'Drunk', 'Ramp Down Bar', 'Freeze Beat 1', 'Gate Repeat 1/32', 'Trap Roll', 'Half Time Bar 2',
      'Reverse Bar 2', 'Shuffle', 'Ping Pong 1/8', 'Slow Down End', 'Stop Start']
VS = ['USER', 'Full', 'Gate 1/16', 'Gate 1/8', 'Gate 1/4', 'Gate Off-beat', 'Side-chain Pump', 'Pump Soft', 'Pump Hard', 'Pump 1/2',
      'Pump 1/8', 'Trance Gate', 'Trance Gate 2', 'Triplet Gate', 'Fade In Bar', 'Fade Out Bar', 'Fade In 2 Bars', 'Fade Out 2 Bars',
      'Tremolo 1/8', 'Tremolo 1/4', 'Swell', 'Mute Beat 4', 'Mute Bar 2', 'Drop Last 1/8', 'Half Volume Off-beats', 'Accent Downbeats',
      'Stutter Fade', 'Ratchet Build', 'Reverse Pump', 'Duck 1/8', 'Random Gate', 'Dotted Gate', 'Saw Up 1/4', 'Saw Down 1/4',
      'Long Swell 2 Bars', 'Kill Switch 1/32', 'Quiet']
assert len(TS) == 37 and len(VS) == 37

def tw(adv, name, t='Live', v='Full', tmix=100, vmix=100, smooth=25, tone=0.3, ramp=0, level=0):
    return preset(adv, name, dict(timeSlot=TS.index(t), volSlot=VS.index(v), timeMix=tmix, volMix=vmix, smooth=smooth,
                                  halfTimeFilter=tone, timeRamp=ramp, level=level))

banks['Time Weaver'] = [
    tw(False, 'Time | Half Time Drop', 'Half Time Bar', tone=0.45),
    tw(False, 'Time | Half Time Two Bars', 'Half Time', tone=0.4),
    tw(False, 'Time | Stutter 1/8', 'Stutter 1/8', smooth=8),
    tw(False, 'Time | Stutter Build', 'Build 1/16 > 1/32', smooth=6),
    tw(False, 'Time | Reverse Bar', 'Reverse Bar', smooth=12),
    tw(False, 'Time | Tape Stop Ending', 'Tape Stop Beat 4', tone=0.6),
    tw(False, 'Time | Trap Roll', 'Trap Roll', smooth=5),
    tw(False, 'Time | Scratch', 'Scratch', smooth=10, tone=0.2),
    tw(False, 'Volume | Side-chain Pump', v='Side-chain Pump', smooth=10),
    tw(False, 'Volume | Trance Gate', v='Trance Gate', smooth=4),
    tw(False, 'Volume | Gate 1/16', v='Gate 1/16', smooth=3),
    tw(False, 'Volume | Tremolo 1/8', v='Tremolo 1/8', smooth=20),
    tw(True, 'Combo | Half Time Pump', 'Half Time Bar', 'Pump Soft', tone=0.45),
    tw(True, 'Combo | Stutter Gate', 'Stutter 1/16', 'Gate 1/8', smooth=4),
    tw(True, 'Combo | Reverse Swell', 'Reverse 1/2 Bar', 'Swell', smooth=15),
    tw(True, 'Combo | Drunk Shuffle', 'Drunk', 'Half Volume Off-beats', smooth=30),
    tw(True, 'Combo | Ratchet Riser', 'Build 1/16 > 1/32', 'Ratchet Build', smooth=4),
    tw(True, 'Combo | Off-beat Echo Duck', 'Off-beat Echo', 'Duck 1/8', tmix=60, smooth=15),
    tw(True, 'Combo | Dotted Echo Gate', 'Dotted Echo', 'Dotted Gate', tmix=70, smooth=10),
    tw(True, 'Combo | Double Time Fade', 'Double Time', 'Fade Out 2 Bars', smooth=20),
    tw(True, 'FX | Freeze Beat 1', 'Freeze Beat 1', smooth=8),
    tw(True, 'FX | Slow Down End', 'Slow Down End', tone=0.5),
    tw(True, 'FX | Stop Start', 'Stop Start', tone=0.4),
    tw(True, 'FX | Ping Pong Repeat', 'Ping Pong 1/8', tmix=80, smooth=10),
    tw(True, 'FX | Parallel Half Time', 'Half Time Bar', tmix=50, tone=0.6),
    tw(True, 'FX | Kill Switch', v='Kill Switch 1/32', smooth=2),
]

# ---------------------------------------------------------------- Larva Filter
import json
LEVELS = json.load(open(os.path.join(os.path.dirname(__file__), 'fx_levels_181.json'))) if os.path.exists(os.path.join(os.path.dirname(__file__), 'fx_levels_181.json')) else {}

def larva(adv, name, routing=0, rate=2, swing=0, seqsmooth=30, seq=1, ing=0, outg=0, mix=100, stages=(), steps=None):
    v = dict(routing=routing, seqRate=rate, seqSwing=swing, seqSmooth=seqsmooth, seqOn=seq, inputGain=ing, outputGain=outg, mix=mix)
    on = set()
    for st in stages:
        i = st['n']; on.add(i); p = 'f%d' % i
        v[p + 'On'] = 1
        for k, pk in (('type', 'Type'), ('cut', 'Cut'), ('res', 'Res'), ('mix', 'Mix'), ('seq', 'Seq'), ('lfo', 'LfoRate'),
                      ('lfoamt', 'LfoAmt'), ('env', 'Env'), ('drive', 'Drive')):
            if k in st: v[p + pk] = st[k]
        if 'seq' not in st: v[p + 'Seq'] = 0
    if 1 not in on: v['f1On'] = 0
    if steps:
        for i, s in enumerate(steps): v['step%d' % (i + 1)] = s
    # output trim so every preset comes out near the level it went in (measured, see fx_levels_181.json)
    v['outputGain'] = max(-24.0, min(24.0, round(v['outputGain'] + LEVELS.get('Larva Filter|' + name, 0.0), 1)))
    return preset(adv, name, v)

def S(n, **k): k['n'] = n; return k
LP12, LP24, HP12, HP24, BAND, NOTCH, PEAK, ALLP = range(8)
r16 = lambda pat: [float(c) / 9.0 for c in pat]

banks['Larva Filter'] = [
    larva(False, 'Sequence | Acid Steps', stages=[S(1, type=LP24, cut=500, res=0.7, seq=0.75, drive=0.3)], steps=r16('9395939591939593')),
    larva(False, 'Sequence | Sixteenth Chop', stages=[S(1, type=LP24, cut=300, res=0.4, seq=0.85)], steps=r16('9090909090909090'), seqsmooth=8),
    larva(False, 'Sequence | Trance Pulse', rate=2, stages=[S(1, type=LP12, cut=700, res=0.5, seq=0.7)], steps=r16('9629629629629629')),
    larva(False, 'Sequence | Half-Note Swell', rate=0, stages=[S(1, type=LP24, cut=400, res=0.3, seq=0.6)], steps=r16('1357913579135791'), seqsmooth=80),
    larva(False, 'Sweep | Slow LFO Low-pass', seq=0, stages=[S(1, type=LP24, cut=900, res=0.4, lfo=0.15, lfoamt=0.6)]),
    larva(False, 'Sweep | Wah Band', seq=0, stages=[S(1, type=BAND, cut=1200, res=0.75, lfo=0.8, lfoamt=0.5)]),
    larva(False, 'Sweep | Auto-Wah Follower', seq=0, stages=[S(1, type=BAND, cut=600, res=0.7, env=0.8)]),
    larva(False, 'Sweep | Rising High-pass', seq=0, stages=[S(1, type=HP24, cut=150, res=0.35, lfo=0.05, lfoamt=0.7)]),
    larva(False, 'Shape | Telephone', seq=0, stages=[S(1, type=HP24, cut=450, res=0.2), S(2, type=LP24, cut=3200, res=0.25), S(3, type=PEAK, cut=1500, res=0.4, mix=0.5)]),
    larva(False, 'Shape | Thin Radio', seq=0, stages=[S(1, type=BAND, cut=1800, res=0.55), S(2, type=HP12, cut=600, res=0.1)]),
    larva(False, 'Shape | Dark Room', seq=0, stages=[S(1, type=LP12, cut=1400, res=0.15), S(2, type=LP12, cut=2600, res=0.1)]),
    larva(False, 'Shape | Notch Phaser', seq=0, stages=[S(1, type=NOTCH, cut=500, res=0.5, lfo=0.3, lfoamt=0.5), S(2, type=NOTCH, cut=1500, res=0.5, lfo=0.37, lfoamt=0.5), S(3, type=ALLP, cut=900, res=0.4, lfo=0.21, lfoamt=0.4)]),
    larva(True, 'Sequence | Stereo Ladder Steps', routing=1, stages=[S(1, type=LP24, cut=400, res=0.6, seq=0.8), S(2, type=BAND, cut=1800, res=0.6, seq=-0.6, mix=0.6)], steps=r16('9174917491749174')),
    larva(True, 'Sequence | Triplet Gate Filter', rate=5, stages=[S(1, type=LP24, cut=250, res=0.45, seq=0.9)], steps=r16('905905905905905'+'9'), seqsmooth=10),
    larva(True, 'Sequence | Swung Funk', swing=35, stages=[S(1, type=BAND, cut=700, res=0.65, seq=0.7, env=0.3)], steps=r16('9049047090490970')),
    larva(True, 'Sequence | Drill Glide', rate=1, stages=[S(1, type=LP24, cut=350, res=0.55, seq=0.7, drive=0.25)], steps=r16('9300930096009300'), seqsmooth=60),
    larva(True, 'Sequence | Eight-Stage Cascade', stages=[S(i, type=[LP24, PEAK, HP12, PEAK, LP12, NOTCH, PEAK, ALLP][i - 1], cut=[300, 800, 120, 2400, 5000, 1000, 3000, 600][i - 1],
                                                              res=0.35, seq=[0.7, -0.5, 0.4, 0.3, -0.3, 0.4, 0.5, 0.2][i - 1], mix=0.85) for i in range(1, 9)],
          steps=r16('9375937593759375')),
    larva(True, 'Split | Lows Steady, Highs Stepped', routing=2, stages=[S(1, type=LP24, cut=250, res=0.2), S(5, type=HP12, cut=2500, res=0.5, seq=0.8)], steps=r16('9090909090909090')),
    larva(True, 'Split | Twin Wah', routing=2, seq=0, stages=[S(1, type=BAND, cut=500, res=0.7, lfo=0.5, lfoamt=0.6), S(5, type=BAND, cut=2000, res=0.7, lfo=0.66, lfoamt=-0.6)]),
    larva(True, 'Parallel | Formant Vowels', routing=1, seq=0, stages=[S(1, type=BAND, cut=700, res=0.8), S(2, type=BAND, cut=1200, res=0.8, mix=0.8), S(3, type=BAND, cut=2600, res=0.8, mix=0.6)]),
    larva(True, 'Parallel | Talking Steps', routing=1, stages=[S(1, type=BAND, cut=700, res=0.8, seq=0.5), S(2, type=BAND, cut=1200, res=0.8, seq=-0.5, mix=0.8)], steps=r16('9517951795179517')),
    larva(True, 'Drive | Screaming Resonance', stages=[S(1, type=LP24, cut=600, res=0.92, seq=0.6, drive=0.7)], steps=r16('9395939591939593'), outg=-6),
    larva(True, 'Drive | Crushed Band', seq=0, stages=[S(1, type=BAND, cut=1000, res=0.6, drive=0.9, env=0.4)], outg=-5),
    larva(True, 'Shape | Shimmer Peaks', seq=0, stages=[S(1, type=PEAK, cut=3000, res=0.6, lfo=0.2, lfoamt=0.3), S(2, type=PEAK, cut=6000, res=0.6, lfo=0.27, lfoamt=-0.3), S(3, type=HP12, cut=200, res=0.1)], mix=80),
    larva(True, 'Shape | Lo-Fi Band', seq=0, stages=[S(1, type=HP12, cut=300, res=0.15), S(2, type=LP12, cut=4500, res=0.2), S(3, type=PEAK, cut=900, res=0.4, mix=0.6, drive=0.3)]),
    larva(True, 'Shape | Gentle Mover', seq=0, stages=[S(1, type=LP12, cut=3500, res=0.2, lfo=0.1, lfoamt=0.35)], mix=70),
]

# ---------------------------------------------------------------- Apex Triad
def triad(adv, name, xl=160, xh=3200, ceil=-0.3, la=2, ing=0, outg=0, mix=100, low=(-6, 0, 20, 250, 6), mid=(-6, 0, 8, 120, 6), high=(-6, 0, 2, 60, 6)):
    v = dict(inputGain=ing, outputGain=outg, mix=mix, xLow=xl, xHigh=xh, ceiling=ceil, lookahead=la)
    for p, b in zip('abc', (low, mid, high)):
        v[p + 'Thr'], v[p + 'Gain'], v[p + 'Att'], v[p + 'Rel'], v[p + 'Knee'] = b
    return preset(adv, name, v)

banks['Apex Triad'] = [
    triad(False, 'Master | Transparent Loud', ing=0, low=(-10, 0, 25, 300, 8), mid=(-10, 0, 10, 150, 8), high=(-10, 0, 3, 80, 8)),
    triad(False, 'Master | Streaming -14', ing=-2, ceil=-1.0, low=(-8, 0, 30, 300, 9), mid=(-8, 0, 12, 160, 9), high=(-8, 0, 4, 90, 9)),
    triad(False, 'Master | Club Loud', ing=3, ceil=-0.2, low=(-14, 1, 15, 200, 6), mid=(-12, 0, 6, 100, 6), high=(-12, 0.5, 2, 50, 6)),
    triad(False, 'Master | Gentle Glue', ing=0, low=(-6, 0, 30, 400, 12), mid=(-6, 0, 15, 200, 12), high=(-6, 0, 5, 100, 12)),
    triad(False, 'Master | Warm Low End', ing=0, low=(-12, 1.5, 20, 280, 8), mid=(-10, 0, 10, 150, 8), high=(-10, -0.5, 3, 80, 8)),
    triad(False, 'Master | Bright Air', ing=0, low=(-10, 0, 25, 300, 8), mid=(-10, 0, 10, 150, 8), high=(-12, 1.5, 2, 60, 8)),
    triad(False, 'Bus | Drum Smash', xl=120, xh=4000, ing=3, mix=70, low=(-18, 1, 10, 120, 4), mid=(-20, 0, 3, 60, 4), high=(-16, 0, 1, 40, 4)),
    triad(False, 'Bus | Vocal Even', xl=250, xh=5000, ing=0, low=(-10, -1, 20, 200, 8), mid=(-14, 0, 6, 100, 8), high=(-16, 0, 1, 50, 8)),
    triad(True, 'Master | Trap Master', xl=120, xh=3500, ing=4, ceil=-0.2, low=(-14, 1, 12, 180, 6), mid=(-12, 0, 6, 100, 6), high=(-12, 0.5, 1.5, 45, 6)),
    triad(True, 'Master | Drill Weight', xl=100, xh=3000, ing=3, low=(-16, 1.5, 10, 160, 5), mid=(-12, 0, 5, 90, 6), high=(-11, 0, 1.5, 45, 6)),
    triad(True, 'Master | Lo-Fi Soft', ing=0, ceil=-1.0, low=(-8, 0, 40, 400, 12), mid=(-8, 0, 20, 220, 12), high=(-10, -1, 6, 120, 12)),
    triad(True, 'Master | Acoustic Natural', ing=0, la=4, low=(-6, 0, 35, 400, 12), mid=(-6, 0, 18, 220, 12), high=(-6, 0, 6, 120, 12)),
    triad(True, 'Master | EDM Wall', xl=150, xh=2800, ing=6, ceil=-0.1, low=(-18, 0.5, 8, 120, 4), mid=(-16, 0, 4, 70, 4), high=(-16, 0, 1, 30, 4)),
    triad(True, 'Master | De-Harsh', ing=0, xh=4500, high=(-20, -1, 0.5, 40, 6)),
    triad(True, 'Bus | Bass Control', xl=90, xh=1200, ing=0, low=(-14, 0, 15, 200, 6), mid=(-10, 0, 8, 120, 6), high=(-6, 0, 3, 60, 6)),
    triad(True, 'Bus | Parallel Crush', ing=6, mix=40, low=(-24, 0, 5, 80, 2), mid=(-24, 0, 2, 50, 2), high=(-24, 0, 0.5, 30, 2)),
]

with open(OUT, 'w') as f:
    f.write('// Generated by scripts/gen_presets_181fx.py - factory presets for the 1.8.1 effects. Do not edit by hand.\n')
    f.write('// Included inside SignaturePresets::bank() (SignaturePresets.h), after SignaturePresets181.inl.\n')
    for name, ps in banks.items():
        f.write('        { "%s", {\n' % name)
        f.write(',\n'.join(ps))
        f.write(' } },\n')
for k, v in banks.items():
    print(k, len(v))
print('wrote', os.path.abspath(OUT))
