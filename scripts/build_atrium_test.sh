#!/bin/bash
# Builds tests/atrium_test.cpp (JUCE core + audio basics + dsp FFT).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; J="$ROOT/tracktion_engine/modules/juce/modules"; OUT="${OUT:-/tmp/spectra_build}"
mkdir -p "$OUT"
printf '#pragma once\n#include <juce_core/juce_core.h>\n#include <juce_audio_basics/juce_audio_basics.h>\n#include <juce_dsp/juce_dsp.h>\n' > "$OUT/JuceHeader.h"
printf 'namespace juce { const char* juce_compilationDate = __DATE__; const char* juce_compilationTime = __TIME__; }\n' > "$OUT/ct.cpp"
D="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DNDEBUG=1 -DJUCE_MODULE_AVAILABLE_juce_audio_formats=1"
for m in juce_core juce_audio_basics juce_audio_formats juce_dsp juce_events juce_data_structures; do
  [ -f "$OUT/$m.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/$m/$m.cpp" -o "$OUT/$m.o" >&2
done
g++ -O2 -std=c++17 $D -I"$OUT" -I"$J" -I"$ROOT/Source" "$ROOT/tests/atrium_test.cpp" "$OUT/ct.cpp" "$OUT"/juce_*.o -lpthread -ldl -o "$OUT/atrium_test" >&2
echo "$OUT/atrium_test"
