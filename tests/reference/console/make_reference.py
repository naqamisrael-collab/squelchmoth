#!/usr/bin/env python3
"""Reference for the console's line transformer: a source through r1 into a one-to-one transformer loaded with r2, the iron
written out as in tests/reference/pentode/make_reference_core.py (Jiles-Atherton, two integrators), no gap. ngspice 42.
Both sides start from nothing with the tone already running and are measured from 1.0 s.
Usage: python3 make_reference.py  -> reference.json and reference.inc"""
import json, subprocess, numpy as np
CORE = dict(LAIR=0.002, MS=150.0, A=0.002, ALPHA=1e-5, K=0.003, C=0.2, V0=0.01)
CIRCUIT = dict(R1=150.0, R2=10000.0)

def netlist(amp, freq, tstop, tstart, tmax, out):
    k, c = CORE, CIRCUIT
    return f"""* line transformer
VIN in 0 SIN(0 {amp} {freq})
R1 in m {c['R1']}
R2 m 0 {c['R2']}
BHH 0 h I = V(m)/({k['LAIR']}*(1+V(f)))
CHH h 0 1
RHH h 0 1e15
BMM 0 mm I = V(f)*V(m)/({k['LAIR']}*(1+V(f)))
CMM mm 0 1
RMM mm 0 1e15
BQ q 0 V = (V(h) + {k['ALPHA']}*V(mm))/{k['A']}
BMAN man 0 V = abs(V(q)) > 1e-4 ? {k['MS']}*(1/tanh(V(q)) - 1/V(q)) : {k['MS']}*V(q)/3
BLP lp 0 V = abs(V(q)) > 1e-4 ? (1/(V(q)*V(q)) - 1/(tanh(V(q))*tanh(V(q))) + 1) : 1/3
BD d 0 V = min(abs(V(man) - V(mm))*0.5*(1 + tanh(sgn(V(man) - V(mm))*V(m)/(2*{k['V0']}))), {0.9*(1-k['C'])*k['K']/k['ALPHA']})
BFIRR firr 0 V = {1-k['C']}*V(d)/({1-k['C']}*{k['K']} - {k['ALPHA']}*V(d))
BBETA beta 0 V = {k['C']}*{k['MS']}/{k['A']}*V(lp)
BF f 0 V = max(0, (V(firr) + V(beta))/(1 - {k['ALPHA']}*V(beta)))
BCORE m 0 I = V(h)
.options reltol=1e-6 abstol=1e-12 vntol=1e-10 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax} uic
linearize v(m) v(h) v(mm)
wrdata {out} v(m) v(h) v(mm)
.endc
.end
"""

def run(amp, freq, cycles, tstart=1.0):
    fs = 384000.0; tstop = tstart + cycles / freq
    open("ref.cir", "w").write(netlist(amp, freq, tstop, tstart, 1.0 / fs, "ref.dat"))
    subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=250)
    d = np.loadtxt("ref.dat"); tt = d[:, 0]; n = int(round(cycles / freq * fs)); ti = tstart + np.arange(n) / fs
    y = np.interp(ti, tt, d[:, 1]); sp = np.fft.rfft(y) / (n / 2)
    flux = CORE['LAIR'] * (d[:, 2] + d[:, 3])
    return [20 * np.log10(abs(sp[k * cycles]) + 1e-300) for k in range(1, 8)], float(np.max(np.abs(flux)))

if __name__ == "__main__":
    out = dict(core=CORE, circuit=CIRCUIT, runs=[])
    for freq, cyc in ((20.0, 8), (50.0, 16), (1000.0, 96)):
        for amp in (0.1, 1.0, 5.0, 15.0):
            h, fl = run(amp, freq, cyc); out["runs"].append(dict(freq=freq, amp=amp, harmonics=[round(x, 3) for x in h], flux=fl))
            print(freq, amp, [round(x, 2) for x in h], "peak flux %.4f" % fl, flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the line transformer; tone from the start, measured from 1.0 s; 384 kHz steps). Do not edit by hand.\n")
        f.write("static const LineRef ref_line[] = { " + ", ".join("{ %s, %s, { %s }, %.6f }" % (r['freq'], r['amp'], ", ".join("%.3f" % x for x in r['harmonics']), r['flux']) for r in out["runs"]) + " };\n")
