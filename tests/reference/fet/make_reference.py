#!/usr/bin/env python3
"""Reference for the FET compressor in ngspice 42 with its own junction-FET model (VTO, BETA, LAMBDA, RS as measured and
published by the FET's maker): (1) the gain cell at fixed control voltages - level and harmonics; (2) the whole loop on a
tone that jumps up and back - the gain reduction against time. Writes reference.json and reference.inc."""
import json, subprocess, os, numpy as np
FET = dict(VTO=-3.15, BETA=2.25e-3, LAMBDA=4.3e-3, RS=23.0)
R = 27000.0
SC = dict(REF=0.1, GAIN=4.0, TH=1.0, DEPTH=0.2, TA=0.0002, TR=0.3, REST=0.02)

def cell_net(control, amp, tstop, tstart, tmax):
    return f"""* FET gain cell
VIN in 0 SIN(0 {amp} 1000)
R1 in n {R}
VC c 0 DC {control}
EG g 0 VALUE = {{0.5*(V(c)+V(n))}}
J1 n g 0 JX
.MODEL JX NJF(VTO={FET['VTO']} BETA={FET['BETA']} LAMBDA={FET['LAMBDA']} RS={FET['RS']})
.options reltol=1e-7 abstol=1e-14 vntol=1e-11 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax}
linearize v(n)
wrdata ref.dat v(n)
.endc
.end
"""

def loop_net(lo, hi, t1, t2, tstop, tmax, sc):
    # a 1 kHz tone at `lo` volts, `hi` volts between t1 and t2
    return f"""* FET compressor loop
BIN in 0 V = (time > {t1} && time < {t2} ? {hi} : {lo}) * sin(6.283185307179586*1000*time)
R1 in n {R}
BC c 0 V = min({2*(FET['VTO']-sc['REST'])} + {2*sc['DEPTH']}*V(x), -0.1)
EG g 0 VALUE = {{0.5*(V(c)+V(n))}}
J1 n g 0 JX
BX 0 x I = max({sc['GAIN']/sc['REF']}*abs(V(n)) - {sc['TH']} - V(x), 0)/{sc['TA']} - V(x)/{sc['TR']}
CX x 0 1
RX x 0 1e15
.MODEL JX NJF(VTO={FET['VTO']} BETA={FET['BETA']} LAMBDA={FET['LAMBDA']} RS={FET['RS']})
.options reltol=1e-6 abstol=1e-13 vntol=1e-10 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} 0 {tmax} uic
linearize v(n) v(x)
wrdata ref.dat v(n) v(x)
.endc
.end
"""

def spice(net):
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(net); subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=280)
    return np.loadtxt("ref.dat")

if __name__ == "__main__":
    out = dict(fet=FET, r=R, sidechain=SC, cell=[], loop=None); fs = 768000.0
    for control in (-6.3, -6.2, -6.0, -5.5, -4.0):
        for amp in (0.01, 0.1, 1.0):
            d = spice(cell_net(control, amp, 0.02 + 0.096, 0.02, 1.0 / fs)); n = int(round(0.096 * fs)); y = np.interp(0.02 + np.arange(n) / fs, d[:, 0], d[:, 1]); sp = np.fft.rfft(y) / (n / 2)
            h = [20 * np.log10(abs(sp[k * 96]) + 1e-300) for k in range(1, 6)]; out["cell"].append(dict(control=control, amp=amp, harmonics=[round(float(x), 3) for x in h]))
            print("cell control %.1f amp %.2f:" % (control, amp), [round(float(x), 2) for x in h], flush=True)
    lo, hi, t1, t2, tstop = 0.02, 0.8, 0.2, 0.5, 1.2
    d = spice(loop_net(lo, hi, t1, t2, tstop, 1.0 / 384000.0, SC))
    times = [0.19, 0.2002, 0.2005, 0.201, 0.203, 0.21, 0.25, 0.49, 0.501, 0.52, 0.6, 0.8, 1.1]
    caps = [float(np.interp(t, d[:, 0], d[:, 2])) for t in times]
    # peak of the node's voltage over the 2 ms before each time
    peaks = [float(np.max(np.abs(d[(d[:, 0] > t - 0.002) & (d[:, 0] <= t), 1]))) for t in times]
    out["loop"] = dict(lo=lo, hi=hi, t1=t1, t2=t2, times=times, capacitor=caps, peaks=peaks)
    for t, c, p in zip(times, caps, peaks): print("loop t %.4f: capacitor %.5f V, node peak %.5f V" % (t, c, p), flush=True)
    json.dump(out, open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the FET gain cell and the compressor loop). Do not edit by hand.\n")
        f.write("static const CellRef ref_cell[] = { " + ", ".join("{ %s, %s, { %s } }" % (r['control'], r['amp'], ", ".join("%.3f" % x for x in r['harmonics'])) for r in out["cell"]) + " };\n")
        f.write("static const LoopRef ref_loop[] = { " + ", ".join("{ %s, %.6f, %.6f }" % (t, c, p) for t, c, p in zip(times, caps, peaks)) + " };\n")
