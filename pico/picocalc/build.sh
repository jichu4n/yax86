#!/bin/bash
# Builds the PicoCalc firmware into build-pico/picocalc and prints where the
# image is and what it costs in flash and SRAM.
#
#   ./build.sh
#
# Environment:
#   PICO_SDK_PATH                Where the Pico SDK lives.
#   YAX86_PICOCALC_DRAW_SCREEN   0 to build an image that does not draw the
#                                guest's screen, into build-pico/picocalc-no-draw,
#                                for measuring what drawing costs. Defaults to 1.
#
# Flash and capture with pico/bench's run.js, which works for any image that
# reports over USB serial and ends with a line saying "done":
#
#   ./pico/bench/run.js build-pico/picocalc/yax86_picocalc.uf2

set -e
cd "$(dirname "${BASH_SOURCE[0]}")"

DRAW_SCREEN="${YAX86_PICOCALC_DRAW_SCREEN:-1}"
BUILD_DIR="$(cd ../.. && pwd)/build-pico/picocalc"
if [ "$DRAW_SCREEN" = 0 ]; then
    BUILD_DIR="$BUILD_DIR-no-draw"
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "!! PICO_SDK_PATH is not set" >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" \
    -DYAX86_PICOCALC_DRAW_SCREEN="$DRAW_SCREEN" >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
read -r flash sram _ < <(../bench/image-size.js "$BUILD_DIR/yax86_picocalc.elf")
printf '%-10s %10s %10s  %s\n' image flash sram uf2
printf '%-10s %10s %10s  %s\n' picocalc "$flash" "$sram" "$BUILD_DIR/yax86_picocalc.uf2"
