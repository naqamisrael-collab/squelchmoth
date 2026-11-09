#!/bin/bash
# Builds a test or calibration tool that uses the voice core (JUCE core + audio basics): $1 = the .cpp, $2 = output name.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; J="$ROOT/tracktion_engine/modules/juce/modules"; OUT="${OUT:-/tmp/masq_build}"
mkdir -p "$OUT"
printf '#pragma once\n#include <juce_core/juce_core.h>\n#include <juce_audio_basics/juce_audio_basics.h>\n' > "$OUT/JuceHeader.h"
printf 'namespace juce { const char* juce_compilationDate = __DATE__; const char* juce_compilationTime = __TIME__; }\n' > "$OUT/ct.cpp"
D="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DNDEBUG=1"
[ -f "$OUT/core.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_core/juce_core.cpp" -o "$OUT/core.o" >&2
[ -f "$OUT/ab.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_audio_basics/juce_audio_basics.cpp" -o "$OUT/ab.o" >&2
SRC="${1:-$ROOT/tests/voice111e_test.cpp}"; NAME="${2:-voice111e_test}"
g++ -O2 -std=c++17 $D -I"$OUT" -I"$J" -I"$ROOT/Source" -I"$ROOT/tests" "$SRC" "$OUT/ct.cpp" "$OUT/core.o" "$OUT/ab.o" -lpthread -ldl -o "$OUT/$NAME" >&2
echo "$OUT/$NAME"
