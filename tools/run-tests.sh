#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -ex

ctest --test-dir build-native/core -j$(nproc) --output-on-failure
core/tests/cpu/cpu_demo_test.sh

# The 8088 CPU hardware test suite. Its data is not in the repository - see
# tools/download-cpu-hardware-tests.sh - and the binary reports no tests at all
# when nothing has been downloaded, so this is a no-op until it is fetched.
# The binary itself is only built when zlib is available.
#
# Set YAX86_REQUIRE_CPU_HARDWARE_TESTS to insist on running it, which CI does.
# Skipping quietly is right on a machine that has not downloaded the data, but
# in CI it would report success while checking nothing.
readonly cpu_hardware_tests=build-native/core/tests/cpu/cpu_hardware_tests

if [ -n "${YAX86_REQUIRE_CPU_HARDWARE_TESTS:-}" ]; then
  if [ ! -x "${cpu_hardware_tests}" ]; then
    echo "${cpu_hardware_tests} was not built - is zlib installed?" >&2
    exit 1
  fi
  if [ ! -s .cache/8088-tests/downloaded.txt ]; then
    echo "No CPU hardware test data - see tools/download-cpu-hardware-tests.sh" >&2
    exit 1
  fi
fi

if [ -x "${cpu_hardware_tests}" ]; then
  "${cpu_hardware_tests}"
fi
