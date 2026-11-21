#!/usr/bin/env python3
"""Reference for the variable-mu compressor in ngspice 42 (the loop includes the output transformer as a 5 Hz high-pass): a push-pull pair of remote-cutoff triodes (each written as two
plate-current laws in parallel, the published form for such tubes), each plate on its own resistor, the output taken
between the plates. (1) The pair at fixed control voltages: gain and harmonics. (2) The whole loop on a tone that jumps
up and back: the control capacitor and the output against time. Writes reference.json and reference.inc."""
import json, subprocess, os, numpy as np
TUBE = dict(MUA=40.0, MUB=40.0 / 3.5, SHARE=0.05, EX=1.3, KG1=1180.0, KP=84.0, KVB=300.0)
CIRCUIT = dict(BPLUS=150.0, RA=22000.0, REST=-1.5, DEEPEST=-12.0)

def pair(t=TUBE, c=CIRCUIT):
    out = f"VB bp 0 DC {c['BPLUS']}\n"
    for x, sign in (("a", "+"), ("b", "-")):
        out += f"R{x} bp p{x} {c['RA']}\nBG{x} g{x} 0 V = {c['REST']} + V(ctl) {sign} 0.5*V(in)\n"
        for n, mu in (("1", t['MUA']), ("2", t['MUB'])):
            out += f"BE{x}{n} e{x}{n} 0 V = V(p{x})/{t['KP']}*ln(1+exp({t['KP']}*(1/{mu}+V(g{x})/sqrt({t['KVB']}+V(p{x})*V(p{x})))))\n"
        out += f"BI{x} p{x} 0 I = {1.0 - t['SHARE']}*2*u(V(e{x}1))*pwr(V(e{x}1),{t['EX']})/{t['KG1']} + {t['SHARE']}*2*u(V(e{x}2))*pwr(V(e{x}2),{t['EX']})/{t['KG1']}\n"
    return out + "BO o0 0 V = V(pb) - V(pa)\n"

def static_net(control, amp, tmax):
    return f"""* variable-mu pair at a fixed control voltage
VIN in 0 SIN(0 {amp} 1000)
VC ctl 0 DC {control}
{pair()}.options reltol=1e-7 abstol=1e-14 vntol=1e-11 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} 0.03 0.02 {tmax}
linearize v(o0)
wrdata ref.dat v(o0)
.endc
.end
"""

def loop_net(lo, hi, t1, t2, tstop, tmax, thresholdVolts, depth, attack, release, c=CIRCUIT):
    return f"""* variable-mu compressor loop
BIN in 0 V = (time > {t1} && time < {t2} ? {hi} : {lo}) * sin(6.283185307179586*1000*time)
BC ctl 0 V = max(-{depth}*V(x), {c['DEEPEST']})
{pair()}CH o0 o 1u
RH o 0 {1.0 / (2 * np.pi * 5.0 * 1e-6)}
BX 0 x I = max(abs(V(o))/{thresholdVolts} - 1 - V(x), 0)/{attack} - V(x)/{release}
CX x 0 1
RX x 0 1e15
.options reltol=1e-6 abstol=1e-13 vntol=1e-10 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} 0 {tmax} uic
linearize v(o) v(x)
wrdata ref.dat v(o) v(x)
.endc
.end
"""

def spice(n):
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(n); r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=3000)
    if not os.path.exists("ref.dat"): print(r.stdout[-1500:], r.stderr[-1500:])
    return np.loadtxt("ref.dat")

if __name__ == "__main__":
    out = dict(tube=TUBE, circuit=CIRCUIT, static=[], loops=[]); fs = 768000.0
    for control in (0.0, -2.0, -4.0, -8.0, -12.0):
        for amp in (0.01, 1.0, 2.0):
            d = spice(static_net(control, amp, 1.0 / fs)); n = int(round(0.01 * fs)); y = np.interp(0.02 + np.arange(n) / fs, d[:, 0], d[:, 1]); sp = np.fft.rfft(y - np.mean(y)) / (n / 2)
            h = [20 * np.log10(abs(sp[k * 10]) + 1e-300) for k in (1, 3, 5)]; out["static"].append(dict(control=control, amp=amp, harmonics=[round(float(v), 4) for v in h]))
            print("static control %.0f amp %.2f:" % (control, amp), [round(float(v), 3) for v in h], flush=True)
    lo, hi, t1, t2, tstop, thr = 0.05, 1.0, 0.2, 1.0, 2.5, 1.5
    times = [0.19, 0.205, 0.22, 0.25, 0.3, 0.5, 0.99, 1.01, 1.1, 1.3, 1.8, 2.4]
    for name, depth in (("compress", 1.9103), ("limit", 16.0162)):
        d = spice(loop_net(lo, hi, t1, t2, tstop, 1.0 / 192000.0, thr, depth, 0.05, 0.6))
        caps = [float(np.interp(t, d[:, 0], d[:, 2])) for t in times]; peaks = [float(np.max(np.abs(d[(d[:, 0] > t - 0.002) & (d[:, 0] <= t), 1]))) for t in times]
        out["loops"].append(dict(name=name, depth=depth, thresholdVolts=thr, capacitor=caps, peaks=peaks))
        for t, c, p in zip(times, caps, peaks): print("%s t %.3f: capacitor %.6f, plates' difference peak %.5f V" % (name, t, c, p), flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the variable-mu pair and the compressor loop). Do not edit by hand.\n")
        f.write("static const StaticRef ref_static[] = { " + ", ".join("{ %s, %s, { %s } }" % (r['control'], r['amp'], ", ".join("%.4f" % v for v in r['harmonics'])) for r in out["static"]) + " };\n")
        for r in out["loops"]:
            f.write("static const LoopRef ref_%s[] = { " % r['name'] + ", ".join("{ %s, %.7f, %.6f }" % (t, c, p) for t, c, p in zip(times, r['capacitor'], r['peaks'])) + " };\n")
