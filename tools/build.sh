#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

# getconf rather than nproc, which macOS does not ship.
jobs="$(getconf _NPROCESSORS_ONLN)"

# One command a line: set -e does not stop on a failure on the left of &&, so
# a configure that failed would be followed by building what was there before.
cmake -B build-native
cmake --build build-native -j"${jobs}"
emcmake cmake -B build-emscripten
cmake --build build-emscripten -j"${jobs}"
