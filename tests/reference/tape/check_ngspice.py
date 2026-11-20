#!/usr/bin/env python3
"""Independent check of make_table.py: the same record process for a few points in ngspice (its own integrator, its own
step control). The bias runs at 1 Hz for convenience - the tape equations have no time scale of their own.
Writes ngspice_points.json."""
import json, subprocess, numpy as np
from make_table import MS, A, ALPHA, K, C, CYCLES, remanence

def run(hs, hb):
    net = f"""* tape element leaving the record gap
BG g 0 V = min(1, time/4) * max(0, 1 - max(0, time - 4)/{CYCLES - 4})
BGD gd 0 V = (time < 4 ? 0.25 : 0) * max(0, 1 - max(0, time - 4)/{CYCLES - 4}) + min(1, time/4) * (time > 4 && time < {CYCLES} ? {-1.0/(CYCLES - 4)} : 0)
BH h 0 V = V(g) * ({hs*A} + {hb*A}*sin(6.283185307179586*time))
BDH dh 0 V = V(gd) * ({hs*A} + {hb*A}*sin(6.283185307179586*time)) + V(g) * {hb*A}*6.283185307179586*cos(6.283185307179586*time)
BQ q 0 V = (V(h) + {ALPHA}*V(m))/{A}
BMAN man 0 V = abs(V(q)) > 1e-4 ? {MS}*(1/tanh(V(q)) - 1/V(q)) : {MS}*V(q)/3
BLP lp 0 V = abs(V(q)) > 1e-4 ? (1/(V(q)*V(q)) - 1/(tanh(V(q))*tanh(V(q))) + 1) : 1/3
BDD dd 0 V = min(max((V(man) - V(m)) * (V(dh) >= 0 ? 1 : -1), 0), {0.9*(1-C)*K/ALPHA})
BFI firr 0 V = {1-C}*V(dd)/({(1-C)*K} - {ALPHA}*V(dd))
BBE beta 0 V = {C*MS/A}*V(lp)
BF f 0 V = max(0, (V(firr) + V(beta))/(1 - {ALPHA}*V(beta)))
BM 0 m I = V(f)*V(dh)
CM m 0 1
RM m 0 1e18
.options reltol=1e-6 abstol=1e-9 vntol=1e-6 method=trap
.control
set wr_singlescale
set numdgt=12
tran {1/512} {CYCLES + 1} 0 {1/512} uic
print v(m)[length(v(m))-1]
.endc
.end
"""
    open("chk.cir", "w").write(net)
    r = subprocess.run(["ngspice", "-b", "chk.cir"], capture_output=True, text=True, timeout=200)
    for l in r.stdout.splitlines():
        if "v(m)[" in l: return float(l.split("=")[1]) / MS
    raise RuntimeError(r.stdout[-500:] + r.stderr[-500:])

if __name__ == "__main__":
    pts = [(0.2, 0.0), (0.5, 0.0), (0.1, 0.05), (0.4, 0.2), (0.8, 0.5), (1.5, 1.0), (2.0, 3.0)]
    out = []
    for hs, hb in pts:
        ng = run(hs, hb); own = float(remanence(hs, hb, 512))
        out.append(dict(hs=hs, hb=hb, ngspice=ng, own=own)); print("hs %.2f hb %.2f: ngspice %.6f  own %.6f  diff %.2e" % (hs, hb, ng, own, ng - own), flush=True)
    json.dump(out, open("ngspice_points.json", "w"), indent=1)
