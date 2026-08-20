#!/bin/bash
# Builds the Pico benchmark firmware and reports what it costs in flash and
# SRAM. Name optimization levels on the command line to build more than one:
# the RP2040 runs code from flash through a 16KB XIP cache, so the level that
# wins on a desktop is not necessarily the one that wins here.
#
#   ./build.sh                 # the default level, -O2
#   ./build.sh O3              # just -O3
#   ./build.sh Os O2 O3        # all three, to compare
#
# Environment:
#   YAX86_CORE_ROOT           Checkout whose core/ is compiled in, for A/B'ing
#                             two core revisions against the same harness.
#                             Defaults to this one.
#   YAX86_PICO_GUEST_RAM_KB   Guest RAM, 128 or 192. Defaults to 128.
#   YAX86_PICO_SYS_CLK_KHZ    System clock. Defaults to the stock 125000.
#   YAX86_PICO_PROFILE        1 to build in the sampling profiler.
#   PICO_SDK_PATH             Where the Pico SDK lives.

set -e
cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_ROOT="$(cd ../.. && pwd)/build-pico"
CORE_ROOT="$(cd "${YAX86_CORE_ROOT:-../..}" && pwd)"
GUEST_RAM_KB="${YAX86_PICO_GUEST_RAM_KB:-128}"
SYS_CLK_KHZ="${YAX86_PICO_SYS_CLK_KHZ:-}"
PROFILE="${YAX86_PICO_PROFILE:-0}"
OPT_LEVELS=()

for arg in "$@"; do
    case "$arg" in
        Os|O2|O3) OPT_LEVELS+=("$arg") ;;
        *)
            echo "Usage: build.sh [Os] [O2] [O3]" >&2
            exit 1
            ;;
    esac
done
if [ ${#OPT_LEVELS[@]} -eq 0 ]; then
    OPT_LEVELS=(O2)
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "!! PICO_SDK_PATH is not set" >&2
    exit 1
fi

for opt in "${OPT_LEVELS[@]}"; do
    echo "==> Building -$opt with ${GUEST_RAM_KB}K guest RAM from $CORE_ROOT"
    cmake -S . -B "$BUILD_ROOT/$opt" \
        -DYAX86_PICO_OPT="$opt" \
        -DYAX86_PICO_GUEST_RAM_KB="$GUEST_RAM_KB" \
        -DYAX86_PICO_SYS_CLK_KHZ="$SYS_CLK_KHZ" \
        -DYAX86_PICO_PROFILE="$PROFILE" \
        -DYAX86_CORE_ROOT="$CORE_ROOT" >/dev/null
    cmake --build "$BUILD_ROOT/$opt" -j"$(nproc)"
done

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
# Flash and SRAM are what has to fit on the board, taken from the program
# headers so that they agree with what the linker printed during the link.
# Flash is what is loaded from it, which includes the initializers for
# everything in .data; SRAM is what is reserved in it, which includes .bss and
# the heap but not the stack, which lives in a scratch bank of its own.
#
# The core's own .text is the figure to line up against a desktop measurement,
# since it leaves out the SDK and the harness.
segment_bytes() {
    arm-none-eabi-readelf -lW "$1" | awk -v want="$2" '
        $1 == "LOAD" {
            file_size = strtonum($5);
            mem_size = strtonum($6);
            virt = strtonum($3);
            phys = strtonum($4);
            if (want == "flash" && phys < 0x20000000) total += file_size;
            if (want == "ram" && virt >= 0x20000000 && virt < 0x20040000) {
                total += mem_size;
            }
        }
        END { print total + 0 }'
}

echo
printf '%-6s %10s %10s %10s  %s\n' opt flash sram core_text uf2
for opt in "${OPT_LEVELS[@]}"; do
    build="$BUILD_ROOT/$opt"
    elf="$build/yax86_pico_bench.elf"
    # Spelled out rather than searched for: CMake names an object after the
    # absolute path of its source, so a build directory reused across two
    # YAX86_CORE_ROOTs holds one object per core, and a search would report
    # whichever it happened to find first.
    core_obj="$build/CMakeFiles/yax86_pico_bench.dir/${CORE_ROOT#/}/core/yax86_core.c.o"
    core_text="$(arm-none-eabi-size "$core_obj" | tail -1 | awk '{print $1}')"
    printf '%-6s %10s %10s %10s  %s\n' \
        "-$opt" "$(segment_bytes "$elf" flash)" "$(segment_bytes "$elf" ram)" \
        "$core_text" "$build/yax86_pico_bench.uf2"
done
