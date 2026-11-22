#!/bin/bash
# Builds tests/drive_test.cpp (no dependencies).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${OUT:-/tmp/spectra_build}"; mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/Source" "$ROOT/tests/drive_test.cpp" -o "$OUT/drive_test" >&2
echo "$OUT/drive_test"
