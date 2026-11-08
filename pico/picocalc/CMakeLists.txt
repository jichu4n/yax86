cmake_minimum_required(VERSION 3.17)

# The yax86 runtime for the ClockworkPi PicoCalc. Standalone rather than a
# subdirectory of the top level build, for the same reason as pico/bench: the
# Pico SDK has to own the toolchain from before project() onwards, and nothing
# else the top level builds cross-compiles.

get_filename_component(YAX86_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# =============================================================================
# Pico SDK
# =============================================================================
# The PicoCalc ships with a Raspberry Pi Pico H, an RP2040, which is also what
# the core's hot path is tuned against.
set(PICO_PLATFORM rp2040 CACHE STRING "Pico platform")
set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(yax86_picocalc C CXX ASM)

# The SDK's own sources need C11; core is C99, which is a subset of it.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -O3 for everything, SDK included: it is what pico/bench reports at and what
# the core's hot path is tuned for.
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_ASM_FLAGS_RELEASE "")

# The whole 4KB scratch bank for the stack, as in pico/bench: -O3 inlines the
# CPU's instruction handlers into deep frames.
add_compile_definitions(PICO_STACK_SIZE=4096)

pico_sdk_init()

# =============================================================================
# Boot floppy image
# =============================================================================
# The MS-DOS 3.30 image pico/bench boots, compiled into flash as a C array for
# when there is no SD card. Generated into the build directory rather than
# committed: at 360KB it is a couple of megabytes of C source.
set(floppy_image "${YAX86_ROOT}/resources/MS-DOS 3.30/DISK01.IMG")
set(floppy_data_prefix "${CMAKE_CURRENT_BINARY_DIR}/generated/dos_floppy_data")
add_custom_command(
    OUTPUT "${floppy_data_prefix}.c" "${floppy_data_prefix}.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${YAX86_ROOT}/core/tools/generate-rom-data-files.js"
        "${floppy_image}" "${floppy_data_prefix}" DOSFloppyData
    DEPENDS "${YAX86_ROOT}/core/tools/generate-rom-data-files.js"
        "${floppy_image}"
    COMMENT "Generating boot floppy data files"
    VERBATIM
)

# =============================================================================
# Firmware
# =============================================================================
add_executable(yax86_picocalc
    src/main.c
    src/font_4x10.c
    src/lcd.c
    src/southbridge.c
    src/text.c
    "${floppy_data_prefix}.c"
    "${YAX86_ROOT}/core/yax86_core.c"
)

target_include_directories(yax86_picocalc PRIVATE
    "${YAX86_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    # Where the generated boot floppy header lands.
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Place the emulator's hot path in SRAM, with the same header pico/bench
# uses, so that the benchmark measures the placement this ships with.
target_compile_options(yax86_picocalc PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/../common/yax86_hot.h")

target_compile_options(yax86_picocalc PRIVATE -Wall -Wextra)

target_link_libraries(yax86_picocalc PRIVATE
    pico_stdlib
    hardware_i2c
    hardware_spi
)

target_link_options(yax86_picocalc PRIVATE -Wl,--print-memory-usage)

# Reports go out over USB CDC. GP0 and GP1, the UART's pins, are left alone.
pico_enable_stdio_usb(yax86_picocalc 1)
pico_enable_stdio_uart(yax86_picocalc 0)

pico_add_extra_outputs(yax86_picocalc)
