#!/usr/bin/env python3
"""Reference for the plate: the thin-plate equation  u_tt = -kappa^2 lap(lap(u)) - 2 sigma u_t + force at one point
stepped directly in time on a grid (fourth-order differences, 6.9 million steps a second), edges held but free to tilt
(u = 0 and lap(u) = 0 there). A short push-pull pulse goes in at one point; the velocity at another is written at
192 kHz. Nothing here knows about modes. Writes reference.json and reference.inc."""
import json, numpy as np
P = dict(LX=0.30, LY=0.20, H=0.0005, E=2.0e11, RHO=7872.0, NU=0.3, SIGMA=30.0, NX=360, NY=240, DRIVE=(188, 128), PICK=(36, 108), T0=0.0006, WIDTH=0.00009, SECONDS=0.025)
kappa = P['H'] * np.sqrt(P['E'] / (12 * P['RHO'] * (1 - P['NU'] ** 2))); h = P['LX'] / P['NX']; assert abs(h - P['LY'] / P['NY']) < 1e-15
over = 36; dt = 1.0 / (192000.0 * over); nsteps = int(P['SECONDS'] * 192000) * over; sig = P['SIGMA']
def odd2(a):   # two ghost rows / columns each side, mirrored with a change of sign (the edge itself is zero)
    a = np.concatenate([-a[2:0:-1, :], a, -a[-2:-4:-1, :]], axis=0); return np.concatenate([-a[:, 2:0:-1], a, -a[:, -2:-4:-1]], axis=1)
def lap(a):
    e = odd2(a); c = e[2:-2, 2:-2]
    return ((-e[0:-4, 2:-2] + 16 * e[1:-3, 2:-2] - 30 * c + 16 * e[3:-1, 2:-2] - e[4:, 2:-2]) + (-e[2:-2, 0:-4] + 16 * e[2:-2, 1:-3] - 30 * c + 16 * e[2:-2, 3:-1] - e[2:-2, 4:])) / (12 * h * h)
u = np.zeros((P['NX'] + 1, P['NY'] + 1)); up = u.copy(); out = []; a = 1 + sig * dt; b = 1 - sig * dt; dx, dy = P['DRIVE']; px, py = P['PICK']
def force(t): x = (t - P['T0']) / P['WIDTH']; return -x * np.exp(-0.5 * x * x)
for n in range(nsteps):
    w = lap(u); w[0, :] = 0; w[-1, :] = 0; w[:, 0] = 0; w[:, -1] = 0
    acc = -kappa * kappa * lap(w); acc[dx, dy] += force(n * dt) / (h * h)
    un = (2 * u - b * up + dt * dt * acc) / a; un[0, :] = 0; un[-1, :] = 0; un[:, 0] = 0; un[:, -1] = 0
    if n % over == 0: out.append((un[px, py] - up[px, py]) / (2 * dt))
    up, u = u, un
    if n % 10000 == 0: print(n, nsteps, float(np.max(np.abs(u))), flush=True)
out = np.array(out); print('samples', len(out), 'peak', float(np.max(np.abs(out))), 'kappa', kappa)
json.dump(dict(params=P, kappa=kappa, rate=192000, out=[float('%.9g' % x) for x in out]), open('reference.json', 'w'))
with open('reference.inc', 'w') as f:
    f.write("// Produced by make_reference.py (the plate's equation stepped directly in time). Do not edit by hand.\n")
    f.write("static const double ref_lx = %s, ref_ly = %s, ref_h = %s, ref_e = %s, ref_rho = %s, ref_nu = %s, ref_sigma = %s, ref_dx = %.12f, ref_dy = %.12f, ref_px = %.12f, ref_py = %.12f, ref_t0 = %s, ref_width = %s;\n" % (P['LX'], P['LY'], P['H'], P['E'], P['RHO'], P['NU'], P['SIGMA'], dx / P['NX'], dy / P['NY'], px / P['NX'], py / P['NY'], P['T0'], P['WIDTH']))
    f.write("static const double ref_out[] = { " + ", ".join("%.9g" % x for x in out) + " };\n")
