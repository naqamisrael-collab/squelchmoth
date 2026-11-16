#!/usr/bin/env python3
"""Squelchmoth preset audit (super prompt 1.11, section 5).

Runs the app's built-in audit, which renders every factory preset of every built-in effect on pink noise, a drum
loop and a sung vocal at 48 kHz and checks level, peak, NaN / Inf, CPU and latency. Writes the Markdown report and
exits non-zero if any preset fails.

    scripts/preset_audit.py                      # all effects -> docs/internal/preset-audit.md
    scripts/preset_audit.py --only "Cinder,Foil Plate" --out docs/internal/preset-audit-1.11.md
"""
import argparse, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BIN = os.path.join(ROOT, "build", "Squelchmoth_artefacts", "Release", "Squelchmoth")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN, help="path to the Squelchmoth binary")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "internal", "preset-audit.md"))
    ap.add_argument("--only", default="", help="comma-separated plug-in names")
    a = ap.parse_args()
    cmd = [a.bin, "--preset-audit", os.path.abspath(a.out)]
    if a.only:
        cmd += ["--only", a.only]
    env = dict(os.environ)
    env.setdefault("DISPLAY", ":99")
    p = subprocess.run(cmd, env=env, capture_output=True, text=True)
    out = p.stdout
    fails = [l for l in out.splitlines() if l.startswith("  FAIL ")]
    summary = re.search(r"== (\d+) plug-ins, (\d+) presets: (\d+) passed, (\d+) failed", out)
    for l in fails:
        print(l)
    if summary:
        print("%s plug-ins, %s presets: %s passed, %s failed" % summary.groups())
    else:
        print(out[-2000:]); print(p.stderr[-2000:])
    print("report:", os.path.abspath(a.out))
    sys.exit(0 if p.returncode == 0 and summary else 1)

if __name__ == "__main__":
    main()
