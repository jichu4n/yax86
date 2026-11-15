#!/bin/bash
# Builds the PicoCalc firmware into build-pico/picocalc (or
# build-pico/pico2/picocalc for a Pico 2) and prints where the image is and
# what it costs in flash and SRAM.
#
#   ./build.sh                            # for pico (RP2040)
#   YAX86_PICO_BOARD=pico2 ./build.sh     # for pico2 (RP2350)
#
# Environment:
#   PICO_SDK_PATH                Where the Pico SDK lives.
#   YAX86_PICO_BOARD             Target board: pico (default) or pico2.
#   YAX86_PICOCALC_DRAW_SCREEN   0 to build an image that does not draw the
#                                guest's screen, into *-no-draw, for measuring
#                                what drawing costs. Defaults to 1.
#   YAX86_PICOCALC_SYS_CLK_KHZ   The system clock. Defaults to 250000; empty
#                                for the board's stock clock (125MHz on pico,
#                                150MHz on pico2).
#
# Flash and capture with pico/bench's run.js, which works for any image that
# reports over USB serial and ends with a line saying "done":
#
#   ./pico/bench/run.js build-pico/picocalc/yax86_picocalc.uf2

set -e
cd "$(dirname "${BASH_SOURCE[0]}")"

BOARD="${YAX86_PICO_BOARD:-pico}"
DRAW_SCREEN="${YAX86_PICOCALC_DRAW_SCREEN:-1}"
SYS_CLK_KHZ="${YAX86_PICOCALC_SYS_CLK_KHZ-250000}"
case "$BOARD" in
    pico) BUILD_DIR="$(cd ../.. && pwd)/build-pico/picocalc" ;;
    pico2) BUILD_DIR="$(cd ../.. && pwd)/build-pico/pico2/picocalc" ;;
    *)
        echo "!! YAX86_PICO_BOARD must be pico or pico2, not '$BOARD'" >&2
        exit 1
        ;;
esac
if [ "$DRAW_SCREEN" = 0 ]; then
    BUILD_DIR="$BUILD_DIR-no-draw"
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "!! PICO_SDK_PATH is not set" >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" \
    -DYAX86_PICO_BOARD="$BOARD" \
    -DYAX86_PICOCALC_DRAW_SCREEN="$DRAW_SCREEN" \
    -DYAX86_PICOCALC_SYS_CLK_KHZ="$SYS_CLK_KHZ" >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
read -r flash sram _ < <(../bench/image-size.js "$BUILD_DIR/yax86_picocalc.elf")
printf '%-10s %10s %10s  %s\n' image flash sram uf2
printf '%-10s %10s %10s  %s\n' picocalc "$flash" "$sram" "$BUILD_DIR/yax86_picocalc.uf2"
