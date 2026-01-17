#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

cmake -B build-native && cmake --build build-native -j$(nproc)
emcmake cmake -B build-emscripten && cmake --build build-emscripten -j$(nproc)

