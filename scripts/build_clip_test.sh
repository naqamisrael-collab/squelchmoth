#!/bin/bash
# Builds tests/clip_test.cpp (no dependencies).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${OUT:-/tmp/spectra_build}"; mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/Source" "$ROOT/tests/clip_test.cpp" -o "$OUT/clip_test" >&2
echo "$OUT/clip_test"
