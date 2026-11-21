#!/usr/bin/env python3
"""Reference for the diode clipper: resistor, a pair of diodes to ground (or two in series one way), capacitor - in ngspice 42
with its own diode model given only IS, N and RS (the numbers the plug-in uses). Writes reference.json and reference.inc."""
import json, subprocess, numpy as np
DIODES = dict(silicon=dict(IS=4.352e-9, N=1.906, RS=0.6458), schottky=dict(IS=315e-9, N=2.03, RS=2.8), sharp=dict(IS=680e-12, N=1.003, RS=12.0))
R, C = 2200.0, 10e-9

def netlist(d, lopsided, amp, freq, tstop, tstart, tmax, out):
    up = "D1 o 0 DX2" if lopsided else "D1 o 0 DX"      # two alike in series are one diode with twice the emission coefficient and twice the resistance
    return f"""* diode clipper
VIN in 0 SIN(0 {amp} {freq})
R1 in o {R}
C1 o 0 {C}
{up}
D2 0 o DX
.MODEL DX D(IS={d['IS']} N={d['N']} RS={d['RS']})
.MODEL DX2 D(IS={d['IS']} N={2*d['N']} RS={2*d['RS']})
.options reltol=1e-7 abstol=1e-14 vntol=1e-10 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax}
linearize v(o)
wrdata {out} v(o)
.endc
.end
"""

def run(d, lopsided, amp, freq=1000.0, cycles=96, settle=0.05):
    fs = 768000.0; tstop = settle + cycles / freq
    open("ref.cir", "w").write(netlist(d, lopsided, amp, freq, tstop, settle, 1.0 / fs, "ref.dat"))
    import os
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=250)
    dd = np.loadtxt("ref.dat"); n = int(round(cycles / freq * fs)); ti = settle + np.arange(n) / fs
    y = np.interp(ti, dd[:, 0], dd[:, 1]); sp = np.fft.rfft(y) / (n / 2)
    return [20 * np.log10(abs(sp[k * cycles]) + 1e-300) for k in range(1, 8)], float(y.max()), float(y.min())

if __name__ == "__main__":
    out = dict(diodes=DIODES, r=R, c=C, runs=[])
    for name, d in DIODES.items():
        for lop in (False, True):
            if lop and name != "silicon": continue
            for amp in (0.1, 0.5, 2.0, 10.0, 40.0):
                h, hi, lo = run(d, lop, amp); out["runs"].append(dict(diode=name, lopsided=lop, amp=amp, harmonics=[round(x, 3) for x in h], top=hi, bottom=lo))
                print(name, "lopsided" if lop else "even", amp, [round(float(x), 2) for x in h], "peaks %.4f %.4f" % (hi, lo), flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the diode clipper; 1 kHz; 768 kHz steps). Do not edit by hand.\n")
        f.write("static const ClipRef ref_clip[] = { " + ", ".join("{ %d, %d, %s, { %s }, %.6f, %.6f }" % (list(DIODES).index(r['diode']), 1 if r['lopsided'] else 0, r['amp'], ", ".join("%.3f" % x for x in r['harmonics']), r['top'], r['bottom']) for r in out["runs"]) + " };\n")
