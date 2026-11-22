#!/bin/bash
# Builds tests/bridge_test.cpp (no dependencies).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${OUT:-/tmp/spectra_build}"; mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/Source" "$ROOT/tests/bridge_test.cpp" -o "$OUT/bridge_test" >&2
echo "$OUT/bridge_test"
