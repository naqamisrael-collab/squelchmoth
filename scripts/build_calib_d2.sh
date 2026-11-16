#!/bin/bash
# Builds tests/preset_calib_d2.cpp (Shard, Murmuration, Masquerade presets through the real DSP) against JUCE core.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; J="$ROOT/tracktion_engine/modules/juce/modules"; OUT="${OUT:-/tmp/masq_build}"
mkdir -p "$OUT"
printf '#pragma once\n#include <juce_core/juce_core.h>\n#include <juce_audio_basics/juce_audio_basics.h>\n' > "$OUT/JuceHeader.h"
printf 'namespace juce { const char* juce_compilationDate = __DATE__; const char* juce_compilationTime = __TIME__; }\n' > "$OUT/ct.cpp"
D="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DNDEBUG=1"
[ -f "$OUT/core.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_core/juce_core.cpp" -o "$OUT/core.o" >&2
[ -f "$OUT/ab.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_audio_basics/juce_audio_basics.cpp" -o "$OUT/ab.o" >&2
g++ -O2 -std=c++17 $D -I"$OUT" -I"$J" -I"$ROOT/Source" -I"$ROOT/tests" "$ROOT/tests/preset_calib_d2.cpp" "$OUT/ct.cpp" "$OUT/core.o" "$OUT/ab.o" -lpthread -ldl -o "$OUT/preset_calib_d2" >&2
echo "$OUT/preset_calib_d2"
