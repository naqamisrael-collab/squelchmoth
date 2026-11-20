#!/usr/bin/env python3
"""Reference for the power stage: a single-ended beam / pentode output stage into a linear output transformer, run in
ngspice with the published pentode equations. Writes reference_<tube>.json (harmonic levels per drive, small-signal
response, resting point). Needs ngspice; the C++ test only reads the numbers.
Usage: python3 make_reference.py classic|heavy|bold"""
import json, subprocess, sys, numpy as np

TUBES = dict(
    classic=dict(MU=8.7, EX=1.35, KG1=1460, KG2=4500, KP=48, KVB=12, CCG=14e-12, CPG1=0.85e-12, CCP=12e-12, RGI=1000),
    heavy=dict(MU=7.9, EX=1.35, KG1=890, KG2=4200, KP=60, KVB=24, CCG=14e-12, CPG1=0.85e-12, CCP=12e-12, RGI=1000),
    bold=dict(MU=8.8, EX=1.35, KG1=730, KG2=4200, KP=32, KVB=16, CCG=14e-12, CPG1=0.85e-12, CCP=12e-12, RGI=1000))
CIRCUIT = dict(BPLUS=350.0, RA=4000.0, RW=150.0, LP=20.0, RK=270.0, CK=220e-6, RS=1000.0, RGS=4700.0, RGL=220e3, CIN=100e-9)

def netlist(t, amp, freq, tstop, tstart, tmax, out):
    c = CIRCUIT
    return f"""* power stage reference
VIN in 0 SIN(0 {amp} {freq})
CIN in a {c['CIN']}
RGL a 0 {c['RGL']}
RGS a g {c['RGS']}
VB bp 0 DC {c['BPLUS']}
RW bp m {c['RW']}
LP m p {c['LP']}
RA m p {c['RA']}
RS bp s {c['RS']}
RK k 0 {c['RK']}
CK k 0 {c['CK']}
* the tube: plate p, grid g, cathode k, screen s
BE1 e1 0 V = V(s,k)/{t['KP']}*ln(1+exp({t['KP']}*(1/{t['MU']}+V(g,k)/V(s,k))))
RE1 e1 0 1MEG
BG1 p k I = 2*u(V(e1))*pwr(V(e1),{t['EX']})/{t['KG1']}*atan(V(p,k)/{t['KVB']})
BG2 s k I = u(V(s,k)/{t['MU']}+V(g,k))*pwr(V(s,k)/{t['MU']}+V(g,k),{t['EX']})/{t['KG2']}
RCP p k 1G
C1 g k {t['CCG']}
C2 p g {t['CPG1']}
C3 p k {t['CCP']}
D3 g5 k DX
R1 g g5 {t['RGI']}
.MODEL DX D(IS=1N RS=1 CJO=10PF TT=1N)
.options reltol=1e-6 abstol=1e-13 vntol=1e-9 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax}
linearize v(m) v(p)
wrdata {out} v(m) v(p)
op
print v(p) v(k) v(s) v(g)
.endc
.end
"""

def run(t, tag, amp, freq, cycles=96, settle=0.6):
    fs = 192000.0 * 4
    tstop = settle + cycles / freq
    open(f"ref_{tag}.cir", "w").write(netlist(t, amp, freq, tstop, settle, 1.0 / fs, f"ref_{tag}.dat"))
    r = subprocess.run(["ngspice", "-b", f"ref_{tag}.cir"], capture_output=True, text=True)
    d = np.loadtxt(f"ref_{tag}.dat"); tt, v = d[:, 0], d[:, 1] - d[:, 2]
    n = int(round(cycles / freq * fs)); ti = settle + np.arange(n) / fs
    y = np.interp(ti, tt, v)
    sp = np.fft.rfft(y) / (n / 2)
    h = [20 * np.log10(abs(sp[k * cycles]) + 1e-300) for k in range(1, 9)]
    op = [l.strip() for l in r.stdout.splitlines() if l.strip().startswith(("v(p)", "v(k)", "v(s)", "v(g)"))]
    return h, op, float(np.mean(y))

if __name__ == "__main__":
    name = sys.argv[1]; t = TUBES[name]
    out = dict(tube=t, circuit=CIRCUIT, harmonics={}, response={}, rest=None)
    for amp in (0.5, 2.0, 5.0, 10.0, 20.0, 40.0):
        h, op, dc = run(t, name, amp, 1000.0)
        out["harmonics"][str(amp)] = [round(x, 3) for x in h]
        if out["rest"] is None: out["rest"] = op
        print(amp, [round(x, 2) for x in h], "dc", round(dc, 5), op if amp == 0.5 else "", flush=True)
    for f in (20.0, 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0):
        cyc = 96 if f >= 100 else (16 if f >= 50 else 8)
        h, _, _ = run(t, name, 0.05, f, cycles=cyc)
        out["response"][str(f)] = round(h[0] - 20 * np.log10(0.05), 3)
        print("gain at", f, "Hz:", out["response"][str(f)], "dB", flush=True)
    json.dump(out, open("reference_%s.json" % name, "w"), indent=1)
