#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

ctest --test-dir build-native/core -j$(nproc) --output-on-failure
core/tests/cpu/cpu_demo_test.sh

# The 8088 CPU hardware test suite. Its data is not in the repository, so fetch
# it first - about 480MB the first time, and nothing on any run after that.
tools/download-cpu-hardware-tests.sh
build-native/core/tests/cpu/cpu_hardware_tests
