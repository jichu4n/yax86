#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

# Must run before ctest, which enumerates the downloaded opcodes at discovery
# time.
tools/download-cpu-hardware-tests.sh

ctest --test-dir build-native/core -j$(nproc) --output-on-failure
core/tests/cpu/cpu_demo_test.sh
