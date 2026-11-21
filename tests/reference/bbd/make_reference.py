#!/usr/bin/env python3
"""Reference for the bucket-brigade line in ngspice 42, as a continuous-time circuit: a third-order Butterworth low-pass,
a sample-and-hold switched once per clock period, the buckets' bend, an ideal delay line of (stages - 1) clock edges, and a
fifth-order Butterworth low-pass. Two tones go in, one of them above half the clock rate so that it folds over.
Writes reference.json and reference.inc (the output at 192 kHz instants)."""
import json, subprocess, os, numpy as np
P = dict(STAGES=64, CLOCK=20000.0, CUTOFF=6000.0, A=0.1461, B=0.6650, F1=1000.0, A1=0.30, F2=13000.0, A2=0.15)

def net(p, tstop, tstep):
    wc = 2 * np.pi * p['CUTOFF']; T = 1.0 / p['CLOCK']; td = (p['STAGES'] - 1) / (2 * p['CLOCK'])
    def biquad(name, src, q):
        return (f"B{name}a 0 {name}a I = {wc}*(V({src}) - V({name}b) - V({name}a)/{q})\nC{name}a {name}a 0 1\nR{name}a {name}a 0 1e15\n"
                f"B{name}b 0 {name}b I = {wc}*V({name}a)\nC{name}b {name}b 0 1\nR{name}b {name}b 0 1e15\n")
    def onepole(name, src):
        return f"B{name} 0 {name} I = {wc}*(V({src}) - V({name}))\nC{name} {name} 0 1\nR{name} {name} 0 1e15\n"
    q5a, q5b = 1 / (2 * np.cos(np.pi / 5)), 1 / (2 * np.cos(2 * np.pi / 5))
    return f"""* bucket-brigade line
BU u 0 V = {p['A1']}*sin(6.283185307179586*{p['F1']}*time) + {p['A2']}*sin(6.283185307179586*{p['F2']}*time)
{onepole('i1', 'u')}{biquad('i2', 'i1', 1.0)}EB bb 0 i2b 0 1
VCLK clk 0 PULSE(0 1 {T - 200e-9} 10n 10n 170n {T})
S1 bb h clk 0 SW
.model SW SW(Ron=1 Roff=1e13 Vt=0.5 Vh=0)
CH h 0 1n
BN n 0 V = V(h) - {p['A']}*V(h)*V(h)*V(h) - {p['B']}*V(h)*V(h)*V(h)*V(h)*V(h)
ED d1 0 n 0 2
RS d1 d2 50
T1 d2 0 d3 0 Z0=50 TD={td}
RL d3 0 50
{onepole('o1', 'd3')}{biquad('o2', 'o1', q5a)}{biquad('o3', 'o2b', q5b)}.options reltol=1e-5 abstol=1e-12 vntol=1e-9 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tstep} {tstop} 0 25n uic
linearize v(o3b) v(n)
wrdata ref.dat v(o3b) v(n)
.endc
.end
"""

if __name__ == "__main__":
    tstop, tstep = 0.04, 1.0 / 768000.0
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(net(P, tstop, tstep)); r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=3000)
    if not os.path.exists("ref.dat"): print(r.stdout[-2000:], r.stderr[-2000:])
    d = np.loadtxt("ref.dat"); n = int(tstop * 192000); y = np.interp(np.arange(n) / 192000.0, d[:, 0], d[:, 1])
    print("samples", n, "peak", float(np.max(np.abs(y))), "held value at 5 ms", float(np.interp(0.005, d[:, 0], d[:, 2])))
    json.dump(dict(params=P, rate=192000, out=[round(float(v), 9) for v in y]), open("reference.json", "w"))
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the bucket-brigade line as a continuous-time circuit). Do not edit by hand.\n")
        f.write("static const double ref_stages = %d, ref_clock = %s, ref_cutoff = %s, ref_f1 = %s, ref_a1 = %s, ref_f2 = %s, ref_a2 = %s;\n" % (P['STAGES'], P['CLOCK'], P['CUTOFF'], P['F1'], P['A1'], P['F2'], P['A2']))
        f.write("static const double ref_out[] = { " + ", ".join("%.8f" % v for v in y) + " };\n")
