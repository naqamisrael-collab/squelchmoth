#!/bin/bash
# Builds tests/strip_test.cpp (no dependencies).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${OUT:-/tmp/spectra_build}"; mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/Source" "$ROOT/tests/strip_test.cpp" -o "$OUT/strip_test" >&2
echo "$OUT/strip_test"
