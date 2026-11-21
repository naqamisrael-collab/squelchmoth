#!/usr/bin/env python3
"""A second, independent reference for the bucket-brigade line: the same chain as make_reference.py worked out by brute
force at 7.68 MHz with scipy (exact discretisation of both filters), reading reference.json for the settings.
Writes bruteforce.inc (the output at 192 kHz instants)."""
import numpy as np, json, scipy.signal as sg
ref = json.load(open('reference.json')); P = ref['params']; ng = np.array(ref['out'])
fsim = 7.68e6; n = int(len(ng) / 192000.0 * fsim); t = np.arange(n) / fsim
u = P['A1'] * np.sin(2 * np.pi * P['F1'] * t) + P['A2'] * np.sin(2 * np.pi * P['F2'] * t); wc = 2 * np.pi * P['CUTOFF']
def butter(order): return sg.zpk2ss(*sg.butter(order, wc, analog=True, output='zpk'))
x = sg.dlsim(sg.cont2discrete(butter(3), 1 / fsim, method='foh'), u)[1][:, 0]
per = int(round(fsim / P['CLOCK'])); vals = x[np.arange(0, n, per)]; vals = vals - P['A'] * vals**3 - P['B'] * vals**5
dly = int(round((P['STAGES'] - 1) / (2 * P['CLOCK']) * fsim)); stair = np.concatenate([np.zeros(dly), np.repeat(vals, per)])[:n]
y = sg.dlsim(sg.cont2discrete(butter(5), 1 / fsim, method='zoh'), stair)[1][:, 0][::40][:len(ng)]
print('brute force against ngspice, worst difference after 3 ms:', float(np.max(np.abs(y[600:] - ng[600:]))))
open('bruteforce.inc', 'w').write("// Produced by make_bruteforce.py (scipy, 7.68 MHz). Do not edit by hand.\nstatic const double brute_out[] = { " + ", ".join("%.9f" % v for v in y) + " };\n")
