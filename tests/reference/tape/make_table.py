#!/usr/bin/env python3
"""The record process of tape, simulated: for a grid of signal fields and bias fields, the magnetisation left on a
tape element after it has passed the record gap. Each element sees the signal field plus a high-frequency bias
field, both dying away together as it leaves the gap; the tape itself is the Jiles-Atherton equations (as read in
Holters and Zolzer, DAFx-16) with the parameter set that paper quotes from Jiles and Atherton 1986
(Ms 1.6e6 A/m, a 1100 A/m, alpha 1.6e-3, k 400 A/m, c 0.2/1.2), fields in units of a.
Writes remanence_table.inc (for the plug-in) and remanence_table.json.   Usage: python3 make_table.py [steps_per_cycle]"""
import json, sys, numpy as np

MS, A, ALPHA, K, C = 1.6e6, 1.1e3, 1.6e-3, 4.0e2, 0.2 / 1.2
CYCLES = 60            # bias cycles while the element leaves the gap
HS = np.linspace(0.0, 2.0, 97)                     # signal field / a
HB = np.array([0.0, 0.025, 0.05, 0.075, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0])     # bias field / a (peak)

def lang(x):
    x = np.asarray(x, float); small = np.abs(x) < 1e-4; xs = np.where(small, 1.0, x)
    return np.where(small, x / 3.0, 1.0 / np.tanh(xs) - 1.0 / xs)
def langd(x):
    x = np.asarray(x, float); small = np.abs(x) < 1e-4; xs = np.where(small, 1.0, x)
    return np.where(small, 1.0 / 3.0, 1.0 / xs**2 - 1.0 / np.tanh(xs)**2 + 1.0)

def slope(h, m, delta):
    """dM/dH (h in A/m, m in A/m), direction delta = +1 / -1."""
    q = (h + ALPHA * m) / A; man = MS * lang(q); d = man - m
    dd = np.minimum(np.maximum(d * delta, 0.0), 0.9 * (1 - C) * K / ALPHA)      # acting only along the direction of travel; the cap keeps the denominator positive (the published form has no guard)
    firr = (1 - C) * dd / ((1 - C) * K - ALPHA * dd)
    beta = C * MS / A * langd(q)
    return np.maximum(0.0, (firr + beta) / (1 - ALPHA * beta))

def remanence(hs, hb, spc=256):
    """hs, hb in units of a (arrays broadcast). Returns M/Ms left when the fields have died away."""
    hs = np.asarray(hs, float) * A; hb = np.asarray(hb, float) * A
    n = CYCLES * spc; m = np.zeros(np.broadcast(hs, hb).shape); hprev = np.zeros_like(m)
    # the element enters the gap: fields rise over a few cycles to full, then die away linearly
    def field(i):
        t = i / spc
        rise = min(1.0, t / 4.0); fall = max(0.0, 1.0 - max(0.0, t - 4.0) / (CYCLES - 4.0))
        g = rise * fall
        return g * (hs + hb * np.sin(2 * np.pi * t))
    for i in range(1, n + 1):
        h = field(i); dh = h - hprev; delta = np.where(dh >= 0, 1.0, -1.0)
        # midpoint (second order) step in H
        k1 = slope(hprev, m, delta); mm = m + 0.5 * dh * k1
        k2 = slope(hprev + 0.5 * dh, mm, delta); m = m + dh * k2; hprev = h
    return m / MS

if __name__ == "__main__":
    spc = int(sys.argv[1]) if len(sys.argv) > 1 else 256
    tab = np.zeros((len(HB), len(HS)))
    for bi, hb in enumerate(HB):
        tab[bi] = remanence(HS, hb, spc)
        print("bias %.1f: slope at origin %.4f, at hs 0.25: %.4f, 0.5: %.4f, 1: %.4f, 2: %.4f" % (hb, (tab[bi][1] - tab[bi][0]) / (HS[1] - HS[0]), np.interp(0.25, HS, tab[bi]), np.interp(0.5, HS, tab[bi]), np.interp(1.0, HS, tab[bi]), tab[bi][-1]), flush=True)
    json.dump(dict(params=dict(MS=MS, A=A, ALPHA=ALPHA, K=K, C=C), cycles=CYCLES, steps_per_cycle=spc, hs=HS.tolist(), hb=HB.tolist(), table=tab.tolist()), open("remanence_table.json", "w"))
    with open("remanence_table.inc", "w") as f:
        f.write("// Produced by tests/reference/tape/make_table.py: magnetisation left on the tape (as a fraction of saturation) against\n// signal field (columns, 0 to 2 in units of the tape's 'a', 97 points) and bias field (rows). Do not edit by hand.\n")
        f.write("static constexpr int remanenceCols = %d, remanenceRows = %d; static constexpr double remanenceHsMax = %.1f;\n" % (len(HS), len(HB), HS[-1]))
        f.write("static constexpr double remanenceBias[%d] = { %s };\n" % (len(HB), ", ".join("%.3f" % b for b in HB)))
        f.write("static constexpr double remanenceTable[%d][%d] = {\n" % (len(HB), len(HS)))
        for row in tab: f.write("    { " + ", ".join("%.7f" % v for v in row) + " },\n")
        f.write("};\n")
