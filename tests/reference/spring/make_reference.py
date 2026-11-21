#!/usr/bin/env python3
"""Reference for the helical spring: the published simplified equations (motion along the wire u, motion towards the axis v)
    u_tt = c^2 (u_ss - k v_s) - 2 sigma u_t + force at the near end
    v_tt = -(c^2 r^2 / 4) (v_ssss + 2 k^2 v_ss + k^4 v) + c^2 k (u_s - k v) - 2 sigma v_t
stepped directly in time on a fine grid (written through the wire's stretch, fourth-order differences in space, 41.7 million steps a second), with the ends
held so that u_s = 0, v = 0, v_ss = 0. A short push-pull pulse goes in at one end; the far end's velocity is written at
192 kHz. Nothing here knows about modes. Writes reference.json and reference.inc."""
import json, numpy as np
P = dict(COIL_D=0.0044, WIRE_D=0.00035, HELIX=0.023, TURNS=43.0, E=2.11e11, RHO=7800.0, SIGMA=40.0, T0=0.0004, WIDTH=0.00006, SECONDS=0.03)
c2 = P['E'] / P['RHO']; r = P['WIRE_D'] / 2; k = 2 / P['COIL_D']; L = float(np.hypot(np.pi * P['COIL_D'] * P['TURNS'], P['HELIX']))
M = 2400; h = L / M; over = 217; dt = 1.0 / (192000.0 * over); nsteps = int(P['SECONDS'] * 192000) * over; sig = P['SIGMA']
# The wire hardly stretches, so the two big terms of the first equation nearly cancel. The grid must keep that
# cancellation exact, so everything is written through the stretch e = u_s - k v, held between the grid points:
#   u_tt = c^2 e_s,   v_tt = -(c^2 r^2 / 4) (d_ss + k^2)^2 v + c^2 k e      (fourth-order differences throughout)
def even(a): return np.concatenate([a[2:0:-1], a, a[-2:-4:-1]])            # two ghost points each side, mirrored
def odd(a): return np.concatenate([-a[2:0:-1], a, -a[-2:-4:-1]])
def oddhalf(e): return np.concatenate([-e[1::-1], e, -e[:-3:-1]])           # the same for values held between points
def to_half_d(x): return (x[0:-3] - 27 * x[1:-2] + 27 * x[2:-1] - x[3:]) / (24 * h)       # derivative, points -> between
def to_half_i(x): return (-x[0:-3] + 9 * x[1:-2] + 9 * x[2:-1] - x[3:]) / 16              # value, points -> between
def d2(x): return (-x[0:-4] + 16 * x[1:-3] - 30 * x[2:-2] + 16 * x[3:-1] - x[4:]) / (12 * h * h)
u = np.zeros(M + 1); v = np.zeros(M + 1); up = u.copy(); vp = v.copy(); out = []; a = 1 + sig * dt; b = 1 - sig * dt
def force(t): x = (t - P['T0']) / P['WIDTH']; return -x * np.exp(-0.5 * x * x)
for n in range(nsteps):
    e = to_half_d(even(u)) - k * to_half_i(odd(v))          # M + 1 + 4 - 3 = M + 2 values: one beyond each end
    e = e[1:-1]; eh = oddhalf(e)                               # M values between the points, then two ghosts each side
    au = c2 * to_half_d(eh); au[0] += force(n * dt) * 2.0 / h
    w = d2(odd(v)) + k * k * v; w[0] = 0.0; w[-1] = 0.0
    av = -(c2 * r * r / 4) * (d2(odd(w)) + k * k * w) + c2 * k * to_half_i(eh); av[0] = 0.0; av[-1] = 0.0
    un = (2 * u - b * up + dt * dt * au) / a; vn = (2 * v - b * vp + dt * dt * av) / a
    if n % over == 0: out.append((un[-1] - up[-1]) / (2 * dt))       # the far end's velocity at time n dt
    up, u, vp, v = u, un, v, vn
    if n % 500000 == 0: print(n, nsteps, flush=True)
out = np.array(out); print('samples', len(out), 'peak', float(np.max(np.abs(out))), 'L', L)
json.dump(dict(params=P, wireLength=L, rate=192000, out=[float('%.9g' % x) for x in out]), open('reference.json', 'w'))
with open('reference.inc', 'w') as f:
    f.write("// Produced by make_reference.py (the spring's equations stepped directly in time). Do not edit by hand.\n")
    f.write("static const double ref_coil = %s, ref_wire = %s, ref_helix = %s, ref_turns = %s, ref_sigma = %s, ref_t0 = %s, ref_width = %s;\n" % (P['COIL_D'], P['WIRE_D'], P['HELIX'], P['TURNS'], P['SIGMA'], P['T0'], P['WIDTH']))
    f.write("static const double ref_out[] = { " + ", ".join("%.9g" % x for x in out) + " };\n")
