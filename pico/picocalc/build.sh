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
#   YAX86_PICOCALC_SYS_CLK_KHZ   The system clock. Defaults to 250000; empty
#                                for the stock 125MHz.
#   YAX86_PICOCALC_GUEST_RAM_KB  Guest RAM with the PSRAM, 128 to 640.
#                                Defaults to 640.
#   YAX86_PICOCALC_PINNED_KB     Guest RAM held in SRAM rather than paged.
#                                Defaults to 0.
#                                Either set builds into build-pico/picocalc-
#                                <guest RAM>k-pin<pinned>k instead.
#
# Flash and capture with pico/bench's run.js, which works for any image that
# reports over USB serial and ends with a line saying "done":
#
#   ./pico/bench/run.js build-pico/picocalc/yax86_picocalc.uf2

set -e
cd "$(dirname "${BASH_SOURCE[0]}")"

DRAW_SCREEN="${YAX86_PICOCALC_DRAW_SCREEN:-1}"
SYS_CLK_KHZ="${YAX86_PICOCALC_SYS_CLK_KHZ-250000}"
GUEST_RAM_KB="${YAX86_PICOCALC_GUEST_RAM_KB:-640}"
PINNED_KB="${YAX86_PICOCALC_PINNED_KB:-0}"
BUILD_DIR="$(cd ../.. && pwd)/build-pico/picocalc"
if [ "$DRAW_SCREEN" = 0 ]; then
    BUILD_DIR="$BUILD_DIR-no-draw"
fi
if [ "$GUEST_RAM_KB" != 640 ] || [ "$PINNED_KB" != 0 ]; then
    BUILD_DIR="$BUILD_DIR-${GUEST_RAM_KB}k-pin${PINNED_KB}k"
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "!! PICO_SDK_PATH is not set" >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" \
    -DYAX86_PICOCALC_DRAW_SCREEN="$DRAW_SCREEN" \
    -DYAX86_PICOCALC_SYS_CLK_KHZ="$SYS_CLK_KHZ" \
    -DYAX86_PICOCALC_GUEST_RAM_KB="$GUEST_RAM_KB" \
    -DYAX86_PICOCALC_PINNED_KB="$PINNED_KB" >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
read -r flash sram _ < <(../bench/image-size.js "$BUILD_DIR/yax86_picocalc.elf")
printf '%-10s %10s %10s  %s\n' image flash sram uf2
printf '%-10s %10s %10s  %s\n' picocalc "$flash" "$sram" "$BUILD_DIR/yax86_picocalc.uf2"
