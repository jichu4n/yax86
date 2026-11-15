#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

# getconf rather than nproc, which macOS does not ship.
jobs="$(getconf _NPROCESSORS_ONLN)"

# One command a line: set -e ignores a failure on the left of &&, so a native
# configure that failed skipped the native build, and the script went on to
# Emscripten and exited 0.
cmake -B build-native
cmake --build build-native -j"${jobs}"
emcmake cmake -B build-emscripten
cmake --build build-emscripten -j"${jobs}"
