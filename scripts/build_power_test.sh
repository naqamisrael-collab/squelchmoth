#!/bin/bash
# Builds tests/power_test.cpp (no dependencies).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${OUT:-/tmp/spectra_build}"; mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/Source" "$ROOT/tests/power_test.cpp" -o "$OUT/power_test" >&2
echo "$OUT/power_test"
