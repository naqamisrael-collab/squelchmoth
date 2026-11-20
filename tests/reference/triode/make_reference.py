#!/usr/bin/env python3
"""Reference for the triode stage: the same circuit run in ngspice with the published triode equations.
Writes reference.json (harmonic levels per drive, small-signal response). Needs ngspice; the C++ test only reads the numbers.
Usage: python3 make_reference.py [bypassed|unbypassed]"""
import json, subprocess, sys, os, numpy as np

TUBE = dict(MU=100, EX=1.4, KG1=1060, KP=600, KVB=300, RGI=2000, CCG=2.3e-12, CGP=2.4e-12, CCP=0.9e-12)
CIRCUIT = dict(BPLUS=300.0, RP=100e3, RK=1500.0, CK=22e-6, RGS=10e3, RGL=1e6, CIN=22e-9, COUT=100e-9, RL=1e6)

def netlist(amp, freq, bypassed, tstop, tstart, tmax, out):
    t, c = TUBE, CIRCUIT
    ck = f"CK k 0 {c['CK']}\n" if bypassed else ""
    return f"""* triode stage reference
VIN in 0 SIN(0 {amp} {freq})
CIN in a {c['CIN']}
RGL a 0 {c['RGL']}
RGS a g {c['RGS']}
VB bp 0 DC {c['BPLUS']}
RP bp p {c['RP']}
RK k 0 {c['RK']}
{ck}COUT p o {c['COUT']}
RL o 0 {c['RL']}
* the triode: plate p, grid g, cathode k
BE1 e1 0 V = V(p,k)/{t['KP']}*ln(1+exp({t['KP']}*(1/{t['MU']}+V(g,k)/sqrt({t['KVB']}+V(p,k)*V(p,k)))))
RE1 e1 0 1G
BG1 p k I = 2*u(V(e1))*pwr(V(e1),{t['EX']})/{t['KG1']}
RCP p k 1G
C1 g k {t['CCG']}
C2 g p {t['CGP']}
C3 p k {t['CCP']}
D3 g5 k DX
R1 g g5 {t['RGI']}
.MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)
.options reltol=1e-6 abstol=1e-13 vntol=1e-9 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax}
linearize v(o)
wrdata {out} v(o)
op
print v(p) v(k) v(g)
.endc
.end
"""

def run(amp, freq, bypassed, cycles=96, settle=0.6):
    fs = 192000.0 * 4
    tstop = settle + cycles / freq
    open("ref.cir", "w").write(netlist(amp, freq, bypassed, tstop, settle, 1.0 / fs, "ref.dat"))
    r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True)
    d = np.loadtxt("ref.dat"); tt, v = d[:, 0], d[:, 1]
    n = int(round(cycles / freq * fs)); ti = settle + np.arange(n) / fs
    y = np.interp(ti, tt, v)
    sp = np.fft.rfft(y) / (n / 2)
    h = [20 * np.log10(abs(sp[k * cycles]) + 1e-300) for k in range(1, 9)]
    ph = float(np.angle(sp[cycles]))
    op = [l for l in r.stdout.splitlines() if l.strip().startswith(("v(p)", "v(k)", "v(g)"))]
    return h, ph, op

if __name__ == "__main__":
    bypassed = (sys.argv[1] if len(sys.argv) > 1 else "bypassed") == "bypassed"
    out = dict(tube=TUBE, circuit=CIRCUIT, bypassed=bypassed, harmonics={}, response={})
    for amp in (0.05, 0.2, 0.5, 1.0, 2.0, 4.0):
        h, ph, op = run(amp, 1000.0, bypassed)
        out["harmonics"][str(amp)] = [round(x, 3) for x in h]
        print(amp, [round(x, 2) for x in h], op if amp == 0.05 else "")
    for f in (20.0, 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0):
        cyc = 96 if f >= 100 else (16 if f >= 50 else 8)
        h, ph, _ = run(0.01, f, bypassed, cycles=cyc)
        out["response"][str(f)] = round(h[0] - 20 * np.log10(0.01), 3)
        print("gain at", f, "Hz:", out["response"][str(f)], "dB")
    json.dump(out, open("reference_%s.json" % ("bypassed" if bypassed else "unbypassed"), "w"), indent=1)
