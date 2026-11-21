#!/usr/bin/env python3
"""Reference for the optical compressor in ngspice 42: the same equations written as a circuit (the cell's fast response
and slow memory as two capacitors, the light-dependent resistor's divider, the panel's light law, the sidechain from the
output) on a 1 kHz tone that jumps up and back. Compress (no bias) and Limit (driver biased off). Writes reference.json
and reference.inc."""
import json, subprocess, os, numpy as np
CELL = dict(GAMMA=0.9, DARK=0.005, LAW=40.0 / 9.0, DRIVER=0.0001, RISE=0.450, FALL=0.035, MEM=0.25, UP=0.7, DOWN=5.0)

def net(lo, hi, t1, t2, tstop, tmax, drive, bias, c=CELL):
    return f"""* optical compressor loop
BIN x 0 V = (time > {t1} && time < {t2} ? {hi} : {lo}) * sin(6.283185307179586*1000*time)
BE e 0 V = max(V(f) + {c['MEM']}*V(m), 0)
BG g 0 V = 1/(1 + {c['DARK']} + (V(e) > 1e-30 ? exp({c['GAMMA']}*ln(V(e))) : 0))
BY y 0 V = V(x)*V(g)
BU u 0 V = {drive}*abs(V(y))*{1.0 + bias} - {bias}
BL l 0 V = V(u) > 1e-30 ? exp({c['LAW']}*ln(V(u))) : 0
BD 0 d I = (V(l) - V(d)) * {1.0 / c['DRIVER']}
CD d 0 1
RD d 0 1e15
BF 0 f I = (V(d) - V(f)) * (V(d) > V(f) ? {1.0 / c['RISE']} : {1.0 / c['FALL']})
CF f 0 1
RF f 0 1e15
BM 0 m I = (V(f) - V(m)) * (V(f) > V(m) ? {1.0 / c['UP']} : {1.0 / c['DOWN']})
CM m 0 1
RM m 0 1e15
.options reltol=1e-6 abstol=1e-13 vntol=1e-10 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} 0 {tmax} uic
linearize v(y) v(f) v(m)
wrdata ref.dat v(y) v(f) v(m)
.endc
.end
"""

def spice(n):
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(n); r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=3000)
    if not os.path.exists("ref.dat"): print(r.stdout[-1500:], r.stderr[-1500:])
    return np.loadtxt("ref.dat")

if __name__ == "__main__":
    lo, hi, t1, t2, tstop, drive = 0.05, 1.0, 0.2, 1.2, 3.0, 4.0
    times = [0.19, 0.201, 0.203, 0.21, 0.25, 0.5, 1.19, 1.21, 1.26, 1.4, 2.0, 2.9]
    out = dict(cell=CELL, lo=lo, hi=hi, t1=t1, t2=t2, drive=drive, times=times, runs=[])
    for bias in (0.0, 8.0):
        d = spice(net(lo, hi, t1, t2, tstop, 1.0 / 192000.0, drive, bias))
        fast = [float(np.interp(t, d[:, 0], d[:, 2])) for t in times]; slow = [float(np.interp(t, d[:, 0], d[:, 3])) for t in times]
        peaks = [float(np.max(np.abs(d[(d[:, 0] > t - 0.002) & (d[:, 0] <= t), 1]))) for t in times]
        out["runs"].append(dict(bias=bias, fast=fast, slow=slow, peaks=peaks))
        for t, a, b, p in zip(times, fast, slow, peaks): print("bias %.0f t %.3f: fast %.6f slow %.6f out peak %.6f" % (bias, t, a, b, p), flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the optical compressor's loop). Do not edit by hand.\n")
        for r in out["runs"]:
            f.write("static const LoopRef ref_bias%d[] = { " % int(r['bias']) + ", ".join("{ %s, %.7f, %.7f, %.7f }" % (t, a, b, p) for t, a, b, p in zip(times, r['fast'], r['slow'], r['peaks'])) + " };\n")
