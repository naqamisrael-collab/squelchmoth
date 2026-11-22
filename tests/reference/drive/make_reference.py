#!/usr/bin/env python3
"""Reference for the overdrive's clipping stage in ngspice 42: an amplifier of gain one million with the ground leg
(4.7 k + 47 nF), and in its feedback the Drive resistance, 51 pF and the diode pair (the silicon diode's published
numbers). Writes the first seven harmonics of the output for a set of tones. Produces reference.json / reference.inc."""
import json, subprocess, os, numpy as np
D = dict(IS=4.352e-9, N=1.906, RS=0.6458)
def net(hz, amp, rf, lopsided, tmax, tstop):
    up = "D1 out m DX\nD3 m inn DX\nRA out m 1e9\nRB m inn 1e9\nCA out m 0.1p\nCB m inn 0.1p\n" if lopsided else "D1 out inn DX\n"
    return f"""* soft clipper: diodes in an amplifier's feedback
VIN inp 0 SIN(0 {amp} {hz})
E1 out 0 inp inn 1e6
RG inn x 4700
CG x 0 47n
RF out inn {rf}
CF out inn 51p
{up}D2 inn out DX
.model DX D(IS={D['IS']} N={D['N']} RS={D['RS']})
.options reltol=1e-7 abstol=1e-15 vntol=1e-12 method=trap temp=27
.control
set wr_singlescale
set numdgt=12
tran {tmax} {tstop} {tstop - 0.02} {tmax}
linearize v(out)
wrdata ref.dat v(out)
.endc
.end
"""
def run(hz, amp, rf, lopsided=False):
    fs = 1536000.0; tstop = 0.06
    if os.path.exists("ref.dat"): os.remove("ref.dat")
    open("ref.cir", "w").write(net(hz, amp, rf, lopsided, 1.0 / fs, tstop)); r = subprocess.run(["ngspice", "-b", "ref.cir"], capture_output=True, text=True, timeout=3000)
    if not os.path.exists("ref.dat"): print(r.stdout[-1500:], r.stderr[-1500:])
    d = np.loadtxt("ref.dat"); n = int(round(0.02 * fs)); t = tstop - 0.02 + np.arange(n) / fs; y = np.interp(t, d[:, 0], d[:, 1]); sp = np.fft.rfft(y - np.mean(y)) / (n / 2); k = int(round(hz * 0.02))
    return [round(float(20 * np.log10(abs(sp[k * h]) + 1e-300)), 4) for h in range(1, 8)]
if __name__ == "__main__":
    out = []
    cases = [(1000, a, rf, False) for rf in (51000.0, 151000.0, 551000.0) for a in (0.02, 0.1, 0.5)] + [(200, 0.2, 151000.0, False), (5000, 0.1, 151000.0, False), (5000, 0.1, 551000.0, False), (1000, 0.1, 151000.0, True), (1000, 0.5, 551000.0, True)]
    for hz, amp, rf, lop in cases:
        h = run(hz, amp, rf, lop); out.append(dict(hz=hz, amp=amp, rf=rf, lopsided=lop, harmonics=h)); print(hz, amp, rf, lop, h, flush=True)
    json.dump(dict(diode=D, cases=out), open("reference.json", "w"), indent=1)
    with open("reference.inc", "w") as f:
        f.write("// Produced by make_reference.py (ngspice 42; the clipping stage). Do not edit by hand.\n")
        f.write("static const DriveRef ref_drive[] = {\n" + ",\n".join("    { %s, %s, %s, %s, { %s } }" % (r['hz'], r['amp'], r['rf'], "true" if r['lopsided'] else "false", ", ".join("%.4f" % v for v in r['harmonics'])) for r in out) + " };\n")
