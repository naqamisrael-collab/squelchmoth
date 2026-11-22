#!/usr/bin/env python3
"""Reference for the diode-bridge compressor in ngspice 42 with its own diode model (the silicon diode's published
numbers): (1) the ring at fixed control currents - level and harmonics of the voltage between its signal corners;
(2) the whole loop (the control current follows the wanted one through the driver's 0.2 ms lag) on a tone that jumps up and back, plain recovery and the automatic (two-capacitor) recovery.
Writes reference.json and reference.inc."""
import json, subprocess, os, numpy as np
D = dict(IS=4.352e-9, N=1.906, RS=0.6458); R = 4700.0; LEAK = 1.0e9; VT = 0.0258649; UNIT = D['N'] * VT / R; LAG = 2.0e-4

def ring():
    return f"""R1 ip a {R}
R2 in b {R}
D1 p a DX
D2 a n DX
D3 p b DX
D4 b n DX
RP p 0 {LEAK}
RN n 0 {LEAK}
.model DX D(IS={D['IS']} N={D['N']} RS={D['RS']})
"""
def static_net(control, amp, tmax):
    return f"""* diode ring at a fixed control current
VP ip 0 SIN(0 {amp / 2} 1000)
VN in 0 SIN(0 {-amp / 2} 1000)
IC n p DC {control}
{ring()}.options reltol=1e-7 abstol=1e-15 vntol=1e-12 method=trap temp=27
.control
set wr_singlescale
set numdgt=12
tran {tmax} 0.03 0.02 {tmax}
linearize v(a) v(b)
wrdata ref.dat v(a) v(b)
.endc
.end
"""
def loop_net(lo, hi, t1, t2, tstop, tmax, thr, ratio, attack, release, slow, charge):
    two = slow > 0
    return f"""* diode-bridge compressor loop
BS s 0 V = (time > {t1} && time < {t2} ? {hi} : {lo}) * sin(6.283185307179586*1000*time)
EP ip 0 s 0 0.5
EN in 0 s 0 -0.5
BH h 0 V = {'max(V(x), V(z))' if two else 'V(x)'}
BW w 0 V = V(h) > 1e-12 ? {UNIT}*exp({ratio - 1.0}*ln(V(h))) : 0
BD 0 c I = (V(w) - V(c))/{LAG}
CD c 0 1
RD c 0 1e15
BC n p I = V(c)
{ring()}BX 0 x I = max(abs(V(a,b))/{thr} - 1 - V(x), 0)/{attack} - V(x)/{release}
CX x 0 1
RX x 0 1e15
BZ 0 z I = {f'max(V(x) - V(z), 0)/{charge} - V(z)/{slow}' if two else '0'}
CZ z 0 1
RZ z 0 1e15
.options reltol=1e-6 abstol=1e-15 vntol=1e-11 method=trap temp=27
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} 0 {tmax} uic
linearize v(a) v(b) v(x) v(z)
wrdata ref.dat v(a) v(b) v(x) v(z)
.endc
.end
"""
def spice(n):
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(n); r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=3000)
    if not os.path.exists("ref.dat"): print(r.stdout[-1500:], r.stderr[-1500:])
    return np.loadtxt("ref.dat")

if __name__ == "__main__":
    out = dict(diode=D, r=R, leak=LEAK, unit=UNIT, static=[], loops=[]); fs = 768000.0
    for mult in (0.0, 0.3, 1.0, 3.0, 9.0, 30.0):
        for amp in (0.001, 0.04, 0.1):
            d = spice(static_net(mult * UNIT, amp, 1.0 / fs)); n = int(round(0.01 * fs)); y = np.interp(0.02 + np.arange(n) / fs, d[:, 0], d[:, 1] - d[:, 2]); sp = np.fft.rfft(y - np.mean(y)) / (n / 2)
            h = [20 * np.log10(abs(sp[k * 10]) + 1e-300) for k in (1, 3, 5)]; out["static"].append(dict(mult=mult, amp=amp, harmonics=[round(float(v), 4) for v in h]))
            print("static I = %.1f units, %.3f V:" % (mult, amp), [round(float(v), 3) for v in h], flush=True)
    lo, hi, t1, t2, tstop, thr = 0.005, 0.04, 0.1, 0.6, 1.5, 0.01
    times = [0.09, 0.102, 0.105, 0.11, 0.13, 0.2, 0.59, 0.605, 0.65, 0.8, 1.1, 1.45]
    for name, ratio, attack, release, slow, charge in (("plain", 4.0, 0.003, 0.4, 0.0, 0.5), ("auto", 2.0, 0.006, 0.1, 2.0, 0.5)):
        d = spice(loop_net(lo, hi, t1, t2, tstop, 1.0 / 192000.0, thr, ratio, attack, release, slow, charge)); y = d[:, 1] - d[:, 2]
        xs = [float(np.interp(t, d[:, 0], d[:, 3])) for t in times]; zs = [float(np.interp(t, d[:, 0], d[:, 4])) for t in times]; peaks = [float(np.max(np.abs(y[(d[:, 0] > t - 0.002) & (d[:, 0] <= t)]))) for t in times]
        out["loops"].append(dict(name=name, ratio=ratio, attack=attack, release=release, slow=slow, charge=charge, threshold=thr, fast=xs, slowcap=zs, peaks=peaks))
        for t, a, b, p in zip(times, xs, zs, peaks): print("%s t %.3f: capacitor %.6f, slow %.6f, output peak %.6f V" % (name, t, a, b, p), flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the diode ring and the compressor loop). Do not edit by hand.\n")
        f.write("static const StaticRef ref_static[] = { " + ", ".join("{ %s, %s, { %s } }" % (r['mult'], r['amp'], ", ".join("%.4f" % v for v in r['harmonics'])) for r in out["static"]) + " };\n")
        for r in out["loops"]: f.write("static const LoopRef ref_%s[] = { " % r['name'] + ", ".join("{ %s, %.7f, %.7f, %.7f }" % (t, a, b, p) for t, a, b, p in zip(times, r['fast'], r['slowcap'], r['peaks'])) + " };\n")
