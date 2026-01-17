#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

ctest --test-dir build-native/core -j$(nproc) --output-on-failure
core/tests/cpu/cpu_demo_test.sh

