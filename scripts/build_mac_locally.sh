#!/bin/bash
# Build Squelchmoth on a Mac with Xcode command line tools (macOS 13+ / Xcode 15+ recommended).
# On macOS Big Sur the newest Xcode (13.2.1) is too old for Tracktion Engine's C++20 code,
# so use the GitHub Actions build instead (see docs/SETUP.md).
set -e
cd "$(dirname "$0")/.."
[ -d tracktion_engine ] || {
  git clone https://github.com/Tracktion/tracktion_engine.git tracktion_engine
  (cd tracktion_engine && git checkout 66417a3cd4aeb206d42056daecc73a1b20684bdc && git submodule update --init --depth 1 modules/juce)
}
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release -j 8
open build/Squelchmoth_artefacts/Release
