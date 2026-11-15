cmake_minimum_required(VERSION 3.17)

# Standalone rather than a subdirectory of the top level build: that one builds
# a native library, the tests and the SDL runtime, none of which cross-compile,
# and the Pico SDK has to own the toolchain from before project() onwards.

# =============================================================================
# Build options
# =============================================================================
# Which board to build for: pico (RP2040) or pico2 (RP2350, Arm).
set(YAX86_PICO_BOARD "pico" CACHE STRING "Target board: pico or pico2")
set_property(CACHE YAX86_PICO_BOARD PROPERTY STRINGS pico pico2)

# The optimization level the whole firmware - emulator, harness and SDK - is
# built at. Worth having as an option rather than fixed: the Pico executes
# code from flash through a small XIP cache, so a larger image can lose to a
# smaller one however the two rank on a desktop.
set(YAX86_PICO_OPT "O2" CACHE STRING "Optimization level: Os, O2 or O3")
set_property(CACHE YAX86_PICO_OPT PROPERTY STRINGS Os O2 O3)
if(NOT YAX86_PICO_OPT MATCHES "^O(s|2|3)$")
    message(FATAL_ERROR
        "YAX86_PICO_OPT must be one of Os, O2 or O3, not '${YAX86_PICO_OPT}'")
endif()

# Optional overclock, in kHz. Left unset the board runs at its stock clock
# (125MHz on pico, 150MHz on pico2).
set(YAX86_PICO_SYS_CLK_KHZ "" CACHE STRING "System clock in kHz, empty for stock")

# Whether to build in the sampling profiler. It costs a little over a percent,
# so a timing run never has it on.
set(YAX86_PICO_PROFILE "0" CACHE STRING "Sample the PC while the workload runs")

# Which checkout's core/ is compiled in. Defaults to this repository, and is
# pointed at a second worktree to A/B one core revision against another.
get_filename_component(YAX86_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(YAX86_CORE_ROOT "${YAX86_ROOT}" CACHE PATH
    "Repository root whose core/ is compiled into the firmware")

# =============================================================================
# Pico SDK
# =============================================================================
# Both must be set before the SDK is pulled in, since pico_sdk_import.cmake
# selects the compiler and platform headers from them.
if(YAX86_PICO_BOARD STREQUAL "pico")
    set(PICO_PLATFORM rp2040 CACHE STRING "Pico platform" FORCE)
    set(PICO_BOARD pico CACHE STRING "Board type" FORCE)
elseif(YAX86_PICO_BOARD STREQUAL "pico2")
    set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico platform" FORCE)
    set(PICO_BOARD pico2 CACHE STRING "Board type" FORCE)
else()
    message(FATAL_ERROR
        "YAX86_PICO_BOARD must be pico or pico2, not '${YAX86_PICO_BOARD}'")
endif()

include(pico_sdk_import.cmake)

project(yax86_pico_bench C CXX ASM)

# The SDK's own sources need C11; core is C99, which is a subset of it.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The optimization level is applied through the Release flags rather than as a
# target option so that the SDK and the C library are built the same way. What
# is being measured is how a whole image behaves in the XIP cache, and an
# emulator compiled one way inside an SDK compiled another would not answer it.
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-${YAX86_PICO_OPT} -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-${YAX86_PICO_OPT} -DNDEBUG")
set(CMAKE_ASM_FLAGS_RELEASE "")

# The default 2KB stack lives in a 4KB scratch bank that nothing else uses, so
# taking all of it costs nothing. -O3 inlines the CPU's instruction handlers
# into deeper frames than the other levels do, and a stack overflow here would
# show up as corruption somewhere else entirely.
add_compile_definitions(PICO_STACK_SIZE=4096)

# The flash divider has to be set before the SDK creates its targets: it is
# compiled into the second stage XIP setup, which is a target of the SDK's own
# and not ours. Setting it on the firmware target does nothing at all, and the
# resulting image hangs at whatever clock the flash could not keep up with.
#
# The divider is against clk_sys, so overclocking without raising it overclocks
# the flash by the same factor, keeping flash at or below its stock rate
# (62.5MHz on pico, 75MHz on pico2). On the RP2350 the SDK omits boot2 unless
# PICO_EMBED_XIP_SETUP=1 is set, leaving the bootrom's fallback XIP timing in
# place and ignoring PICO_FLASH_SPI_CLKDIV altogether.
if(YAX86_PICO_BOARD STREQUAL "pico2")
    add_compile_definitions(PICO_EMBED_XIP_SETUP=1)
    if(YAX86_PICO_SYS_CLK_KHZ GREATER 450000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=8)
    elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 300000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=6)
    elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 150000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=4)
    endif()
else()
    if(YAX86_PICO_SYS_CLK_KHZ GREATER 400000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=8)
    elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 250000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=6)
    elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 125000)
        add_compile_definitions(PICO_FLASH_SPI_CLKDIV=4)
    endif()
endif()

pico_sdk_init()

# =============================================================================
# Boot floppy image
# =============================================================================
# The same MS-DOS 3.30 image the SDL runtime mounts, compiled into flash as a C
# array by the generator the BIOS and option ROM images use, because this
# target has no file system to read it from.
#
# Generated into the build directory rather than committed the way the ROMs'
# arrays are: at 360KB the image is a couple of megabytes of C source.
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
add_executable(yax86_pico_bench
    src/main.c
    ../common/pager.c
    ../common/psram.c
    "${floppy_data_prefix}.c"
    "${YAX86_CORE_ROOT}/core/yax86_core.c"
)

target_include_directories(yax86_pico_bench PRIVATE
    "${YAX86_CORE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/boards/${YAX86_PICO_BOARD}"
    # Where the generated boot floppy header lands.
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(yax86_pico_bench PRIVATE
    YAX86_PICO_OPT="-${YAX86_PICO_OPT}"
    YAX86_PICO_PROFILE=${YAX86_PICO_PROFILE}
)
if(YAX86_PICO_SYS_CLK_KHZ)
    target_compile_definitions(yax86_pico_bench PRIVATE
        YAX86_PICO_SYS_CLK_KHZ=${YAX86_PICO_SYS_CLK_KHZ})
endif()

# Place the emulator's per-instruction hot path in SRAM. The core marks it with
# Y_HOT, which expands to nothing unless something defines it; this is where
# this target does. Force-included rather than included from the core, so that
# the core knows nothing about this target. Shared with pico/picocalc, so that
# the benchmark measures the placement the runtime ships with.
target_compile_options(yax86_pico_bench PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/../common/yax86_hot.h")

target_compile_options(yax86_pico_bench PRIVATE -Wall -Wextra)

target_link_libraries(yax86_pico_bench PRIVATE
    pico_stdlib hardware_pio hardware_vreg)

# The PicoCalc's PSRAM, which guest RAM is paged through where it is present.
# See pico/common/boards/<board>/yax86_pico.h.
pico_generate_pio_header(yax86_pico_bench
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/psram.pio")

# How much flash and SRAM the image needs, per region, at the end of the link.
target_link_options(yax86_pico_bench PRIVATE -Wl,--print-memory-usage)

# Results go out over USB CDC. The UART is left off: it is a blocking write at
# a fixed baud rate, which is exactly the kind of host I/O that would swamp
# what this measures if it ever crept into the run.
pico_enable_stdio_usb(yax86_pico_bench 1)
pico_enable_stdio_uart(yax86_pico_bench 0)

# .uf2 to flash with, plus the .map and .dis that say where the space went.
pico_add_extra_outputs(yax86_pico_bench)
