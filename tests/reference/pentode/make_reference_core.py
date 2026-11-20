#!/usr/bin/env python3
"""Reference for the power stage with a saturating, hysteretic transformer core (Jiles-Atherton, gapped), in ngspice.
The core is written out with behavioural sources: two integrators (field h, magnetisation m) and the published
differential equation. Cold start: everything discharged, the supply ramps up in 10 ms; then the tone.
Usage: python3 make_reference_core.py   -> reference_core.json"""
import json, subprocess, sys, numpy as np

TUBE = dict(MU=8.7, EX=1.35, KG1=1460, KG2=4500, KP=48, KVB=12, CCG=14e-12, CPG1=0.85e-12, CCP=12e-12, RGI=1000)
CIRCUIT = dict(BPLUS=350.0, RA=4000.0, RW=150.0, RK=270.0, CK=220e-6, RS=1000.0, RGS=4700.0, RGL=220e3, CIN=100e-9)
# core in electrical units: h and m in amperes (of magnetising current), flux linkage lam = LAIR (h + m), gap current lam / LG
CORE = dict(LAIR=0.02, MS=150.0, A=0.0025, ALPHA=1e-5, K=0.004, C=0.2, LG=22.0, V0=0.01)
RAMP = 0.010

def netlist(amp, freq, tone_at, tstop, tstart, tmax, out, core=CORE):
    t, c, k = TUBE, CIRCUIT, core
    return f"""* power stage with a hysteretic core
VIN in 0 SIN(0 {amp} {freq} {tone_at})
CIN in a {c['CIN']}
RGL a 0 {c['RGL']}
RGS a g {c['RGS']}
VB bp 0 PWL(0 0 {RAMP} {c['BPLUS']})
RW bp m {c['RW']}
RA m p {c['RA']}
RS bp s {c['RS']}
RK k 0 {c['RK']}
CK k 0 {c['CK']}
* the core between m and p
BHH 0 h I = V(m,p)/({k['LAIR']}*(1+V(f)))
CHH h 0 1
RHH h 0 1e15
BMM 0 mm I = V(f)*V(m,p)/({k['LAIR']}*(1+V(f)))
CMM mm 0 1
RMM mm 0 1e15
BLAM lam 0 V = {k['LAIR']}*(V(h) + V(mm))
BQ q 0 V = (V(h) + {k['ALPHA']}*V(mm))/{k['A']}
BMAN man 0 V = abs(V(q)) > 1e-4 ? {k['MS']}*(1/tanh(V(q)) - 1/V(q)) : {k['MS']}*V(q)/3
BLP lp 0 V = abs(V(q)) > 1e-4 ? (1/(V(q)*V(q)) - 1/(tanh(V(q))*tanh(V(q))) + 1) : 1/3
BD d 0 V = abs(V(man) - V(mm))*0.5*(1 + tanh(sgn(V(man) - V(mm))*V(m,p)/(2*{k['V0']})))
BFIRR firr 0 V = {1-k['C']}*V(d)/({1-k['C']}*{k['K']} - {k['ALPHA']}*V(d))
BBETA beta 0 V = {k['C']}*{k['MS']}/{k['A']}*V(lp)
BF f 0 V = (V(firr) + V(beta))/(1 - {k['ALPHA']}*V(beta))
BCORE m p I = V(h) + V(lam)/{k['LG']}
* the tube: plate p, grid g, cathode k, screen s
BE1 e1 0 V = max(V(s,k),1e-3)/{t['KP']}*ln(1+exp({t['KP']}*(1/{t['MU']}+V(g,k)/max(V(s,k),1e-3))))
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
.options reltol=1e-6 abstol=1e-13 vntol=1e-11 method=trap
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstart} {tmax} uic
linearize v(m) v(p) v(lam) v(mm) v(h)
wrdata {out} v(m) v(p) v(lam) v(mm) v(h)
.endc
.end
"""

def run(amp, freq, cycles, tone_at=0.6, settle=0.4, tag="core"):
    fs = 192000.0 * 2
    tstart = tone_at + settle; tstop = tstart + cycles / freq
    open(f"ref_{tag}.cir", "w").write(netlist(amp, freq, tone_at, tstop, tstart, 1.0 / fs, f"ref_{tag}.dat"))
    r = subprocess.run(["ngspice", "-b", f"ref_{tag}.cir"], capture_output=True, text=True)
    d = np.loadtxt(f"ref_{tag}.dat"); tt = d[:, 0]; v = d[:, 1] - d[:, 2]
    n = int(round(cycles / freq * fs)); ti = tstart + np.arange(n) / fs
    y = np.interp(ti, tt, v)
    sp = np.fft.rfft(y) / (n / 2)
    h = [20 * np.log10(abs(sp[k * cycles]) + 1e-300) for k in range(1, 9)]
    return h, dict(lam=float(np.interp(tstart, tt, d[:, 3])), m=float(np.interp(tstart, tt, d[:, 4])), h=float(np.interp(tstart, tt, d[:, 5])), lam_pp=float(d[:, 3].max() - d[:, 3].min())), r

if __name__ == "__main__":
    out = dict(tube=TUBE, circuit=CIRCUIT, core=CORE, ramp=RAMP, tone_at=0.6, settle=0.4, runs=[])
    todo = [(50.0, 8, a) for a in (1.0, 5.0, 10.0, 20.0)] + [(1000.0, 96, a) for a in (2.0, 10.0, 20.0)] if len(sys.argv) < 2 else [(float(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3]))]
    for f, cyc, amp in todo:
        h, st, r = run(amp, f, cyc, tag=f"core_{int(f)}_{amp}")
        out["runs"].append(dict(freq=f, amp=amp, harmonics=[round(x, 3) for x in h], state=st))
        print(f, amp, [round(x, 2) for x in h], st, flush=True)
    if len(sys.argv) < 2: json.dump(out, open("reference_core.json", "w"), indent=1)
