cmake_minimum_required(VERSION 3.17)

# Standalone rather than a subdirectory of the top level build: that one builds
# a native library, the tests and the SDL runtime, none of which cross-compile,
# and the Pico SDK has to own the toolchain from before project() onwards.

# =============================================================================
# Build options
# =============================================================================
# The optimization level the whole firmware - emulator, harness and SDK - is
# built at. Worth having as an option rather than fixed: the Cortex-M0+ runs
# code from flash through a 16KB XIP cache, so a larger image can lose to a
# smaller one however the two rank on a desktop.
set(YAX86_PICO_OPT "O2" CACHE STRING "Optimization level: Os, O2 or O3")
set_property(CACHE YAX86_PICO_OPT PROPERTY STRINGS Os O2 O3)
if(NOT YAX86_PICO_OPT MATCHES "^O(s|2|3)$")
    message(FATAL_ERROR
        "YAX86_PICO_OPT must be one of Os, O2 or O3, not '${YAX86_PICO_OPT}'")
endif()

# Guest conventional memory. Period-accurate XT sizes only, bounded below by
# what MS-DOS 3.30 needs to boot and above by what fits in the RP2040's 264KB
# of SRAM alongside video memory, the SDK and the USB stack.
set(YAX86_PICO_GUEST_RAM_KB "128" CACHE STRING "Guest RAM in KB: 128 or 192")
set_property(CACHE YAX86_PICO_GUEST_RAM_KB PROPERTY STRINGS 128 192)
if(NOT YAX86_PICO_GUEST_RAM_KB MATCHES "^(128|192)$")
    message(FATAL_ERROR
        "YAX86_PICO_GUEST_RAM_KB must be 128 or 192, not "
        "'${YAX86_PICO_GUEST_RAM_KB}'.")
endif()

# Optional overclock, in kHz. Left unset the board runs at its stock 125MHz.
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
# Both must be set before the SDK is pulled in. The RP2040's small XIP cache is
# a large part of why this harness exists, so the board is not a variable.
set(PICO_PLATFORM rp2040 CACHE STRING "Pico platform")
set(PICO_BOARD pico CACHE STRING "Board type")

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
# compiled into the second stage bootloader, which is a target of the SDK's own
# and not ours. Setting it on the firmware target does nothing at all, and the
# resulting image hangs at whatever clock the flash could not keep up with.
#
# The divider is against clk_sys, so overclocking without raising it overclocks
# the flash by the same factor. At 400MHz and the default divider of 2 that is
# a 200MHz SPI clock, which no RP2040 flash part will do.
if(YAX86_PICO_SYS_CLK_KHZ GREATER 400000)
    add_compile_definitions(PICO_FLASH_SPI_CLKDIV=8)
elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 250000)
    add_compile_definitions(PICO_FLASH_SPI_CLKDIV=6)
elseif(YAX86_PICO_SYS_CLK_KHZ GREATER 125000)
    add_compile_definitions(PICO_FLASH_SPI_CLKDIV=4)
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
    "${floppy_data_prefix}.c"
    "${YAX86_CORE_ROOT}/core/yax86_core.c"
)

target_include_directories(yax86_pico_bench PRIVATE
    "${YAX86_CORE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    # Where the generated boot floppy header lands.
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(yax86_pico_bench PRIVATE
    YAX86_PICO_GUEST_RAM_KB=${YAX86_PICO_GUEST_RAM_KB}
    YAX86_PICO_OPT="-${YAX86_PICO_OPT}"
    YAX86_PICO_PROFILE=${YAX86_PICO_PROFILE}
)
if(YAX86_PICO_SYS_CLK_KHZ)
    target_compile_definitions(yax86_pico_bench PRIVATE
        YAX86_PICO_SYS_CLK_KHZ=${YAX86_PICO_SYS_CLK_KHZ})
endif()

target_compile_options(yax86_pico_bench PRIVATE -Wall -Wextra)

target_link_libraries(yax86_pico_bench PRIVATE pico_stdlib hardware_vreg)

# How much flash and SRAM the image needs, per region, at the end of the link.
target_link_options(yax86_pico_bench PRIVATE -Wl,--print-memory-usage)

# Results go out over USB CDC. The UART is left off: it is a blocking write at
# a fixed baud rate, which is exactly the kind of host I/O that would swamp
# what this measures if it ever crept into the run.
pico_enable_stdio_usb(yax86_pico_bench 1)
pico_enable_stdio_uart(yax86_pico_bench 0)

# .uf2 to flash with, plus the .map and .dis that say where the space went.
pico_add_extra_outputs(yax86_pico_bench)
