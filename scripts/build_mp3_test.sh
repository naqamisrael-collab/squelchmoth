#!/bin/bash
# Builds tests/mp3_test.cpp (Squelchmoth's MP3 encoder vs JUCE's MP3 decoder) and prints its path.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; J="$ROOT/tracktion_engine/modules/juce/modules"; OUT="${OUT:-/tmp/mp3_build}"
mkdir -p "$OUT"
printf '#pragma once\n#include <juce_core/juce_core.h>\n#include <juce_audio_basics/juce_audio_basics.h>\n#include <juce_audio_formats/juce_audio_formats.h>\n' > "$OUT/JuceHeader.h"
printf 'namespace juce { const char* juce_compilationDate = __DATE__; const char* juce_compilationTime = __TIME__; }\n' > "$OUT/ct.cpp"
D="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DNDEBUG=1 -DJUCE_USE_MP3AUDIOFORMAT=1 -DJUCE_USE_FLAC=0 -DJUCE_USE_OGGVORBIS=0"
[ -f "$OUT/core.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_core/juce_core.cpp" -o "$OUT/core.o" >&2
[ -f "$OUT/ab.o" ] || g++ -O2 -std=c++17 $D -I"$J" -c "$J/juce_audio_basics/juce_audio_basics.cpp" -o "$OUT/ab.o" >&2
[ -f "$OUT/af.o" ] || g++ -O2 -std=c++17 $D -I"$OUT" -I"$J" -c "$J/juce_audio_formats/juce_audio_formats.cpp" -o "$OUT/af.o" >&2
g++ -O2 -std=c++17 $D -I"$OUT" -I"$J" -I"$ROOT/Source" "$ROOT/tests/mp3_test.cpp" "$OUT/ct.cpp" "$OUT/core.o" "$OUT/ab.o" "$OUT/af.o" -lpthread -ldl -o "$OUT/mp3_test" >&2
echo "$OUT/mp3_test"
