cmake_minimum_required(VERSION 3.17)

# The yax86 runtime for the ClockworkPi PicoCalc. Standalone rather than a
# subdirectory of the top level build, for the same reason as pico/bench: the
# Pico SDK has to own the toolchain from before project() onwards, and nothing
# else the top level builds cross-compiles.

get_filename_component(YAX86_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Whether the guest's screen is drawn on the LCD. Off gives an image identical
# but for that, which is how what drawing costs is measured.
set(YAX86_PICOCALC_DRAW_SCREEN "1" CACHE STRING "Draw the guest's screen: 1 or 0")
if(NOT YAX86_PICOCALC_DRAW_SCREEN MATCHES "^[01]$")
    message(FATAL_ERROR
        "YAX86_PICOCALC_DRAW_SCREEN must be 1 or 0, not "
        "'${YAX86_PICOCALC_DRAW_SCREEN}'")
endif()

# The system clock, in kHz, or empty for the stock 125MHz. 250MHz is what the
# emulator needs to run faster than a real PC/XT, and is what pico/bench
# reports at.
set(YAX86_PICOCALC_SYS_CLK_KHZ "250000" CACHE STRING
    "System clock in kHz, empty for stock")
if(YAX86_PICOCALC_SYS_CLK_KHZ STREQUAL "")
    set(sys_clk_khz 0)
elseif(YAX86_PICOCALC_SYS_CLK_KHZ MATCHES "^[0-9]+$"
       AND NOT YAX86_PICOCALC_SYS_CLK_KHZ GREATER 400000)
    set(sys_clk_khz ${YAX86_PICOCALC_SYS_CLK_KHZ})
else()
    message(FATAL_ERROR
        "YAX86_PICOCALC_SYS_CLK_KHZ must be empty or at most 400000, not "
        "'${YAX86_PICOCALC_SYS_CLK_KHZ}'")
endif()

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

# The flash's SPI divider is against clk_sys, so an overclock raises it with
# the clock, as in pico/bench. It goes in the second stage bootloader, a
# target of the SDK's own, which only add_compile_definitions() reaches.
if(sys_clk_khz GREATER 250000)
    add_compile_definitions(PICO_FLASH_SPI_CLKDIV=6)
elseif(sys_clk_khz GREATER 125000)
    add_compile_definitions(PICO_FLASH_SPI_CLKDIV=4)
endif()


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
# FatFs
# =============================================================================
# ChaN's release, as downloaded from elm-chan.org, unpacked into the build
# directory. ff.h includes ffconf.h from its own directory before any include
# path, so ours is copied over the stock one; configure_file() also reruns the
# configure when ours changes.
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(
    fatfs
    URL "${YAX86_ROOT}/resources/FatFs/ff16.zip"
    URL_HASH SHA256=99f7dc1f7e095356e4a9e3dbe29959090d8b948afe2bbc5441e52fdf4b85449e
)
FetchContent_MakeAvailable(fatfs)
set(fatfs_dir "${fatfs_SOURCE_DIR}/source")
configure_file(src/ffconf.h "${fatfs_dir}/ffconf.h" COPYONLY)

# =============================================================================
# Firmware
# =============================================================================
add_executable(yax86_picocalc
    src/main.c
    src/emulator.c
    src/stack_usage.c
    src/emulator_menu_screen.c
    src/floppy_drive_screen.c
    src/question_screen.c
    src/status_screen.c
    src/browser_screen.c
    src/init.c
    src/debug_command.c
    src/disk_image.c
    src/diskio.c
    src/display.c
    src/file_list.c
    src/fs.c
    src/config_file.c
    src/settings.c
    src/settings_screen.c
    src/vm_config.c
    src/vm_list.c
    src/floppy.c
    src/harddisk.c
    src/font_4x10.c
    src/font_screen.c
    src/keymap.c
    src/launcher.c
    src/launcher_menu_screen.c
    src/vm_manager.c
    src/vm_manager_screen.c
    src/lcd.c
    ../common/pager.c
    ../common/psram.c
    src/runtime_log.c
    src/screen.c
    src/sdcard.c
    src/southbridge.c
    src/speaker.c
    src/status_bar.c
    src/emulator_status_bar.c
    src/running_vm.c
    src/text.c
    src/typing.c
    src/box.c
    src/file_browser.c
    src/list_box.c
    src/usb_storage_screen.c
    src/usb_storage_active_screen.c
    src/usb_storage_disconnect_screen.c
    src/vm_delete_screen.c
    src/vm_drive_config_screen.c
    src/vm_config_screen.c
    src/vm_name_screen.c
    src/vm_screen.c
    src/launcher_status_bar.c
    src/usb_descriptors.c
    src/usb_storage.c
    "${fatfs_dir}/ff.c"
    "${floppy_data_prefix}.c"
    "${YAX86_ROOT}/core/yax86_core.c"
)

target_include_directories(yax86_picocalc PRIVATE
    "${YAX86_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${fatfs_dir}"
    # Where the generated boot floppy header lands.
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(yax86_picocalc PRIVATE
    YAX86_PICOCALC_DRAW_SCREEN=${YAX86_PICOCALC_DRAW_SCREEN}
    YAX86_PICOCALC_SYS_CLK_KHZ=${sys_clk_khz}
    # What pico_usb_reset gives a device it describes itself, and turns off for
    # one described by the application: picotool's reset interface, with the
    # Microsoft OS 2.0 descriptor Windows needs to use it without a driver, and
    # a reset into BOOTSEL when the serial port is opened at 1200 baud.
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
    PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
    # And what pico_stdio_usb does for a device it describes, and leaves to the
    # application otherwise: starting TinyUSB, and running its task from a low
    # priority interrupt. Without them the board never appears on USB at all.
    PICO_STDIO_USB_ENABLE_TINYUSB_INIT=1
    PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=1
)

# Place the emulator's hot path in SRAM, with the same header pico/bench
# uses, so that the benchmark measures the placement this ships with.
target_compile_options(yax86_picocalc PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/../common/yax86_hot.h")

target_compile_options(yax86_picocalc PRIVATE -Wall -Wextra)

target_link_libraries(yax86_picocalc PRIVATE
    pico_stdlib
    pico_multicore
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_spi
    hardware_vreg
    hardware_watchdog
    pico_unique_id
    # Linked directly, rather than only through pico_stdio_usb, so that the
    # USB device is ours to describe - see src/usb_descriptors.c.
    tinyusb_device
)

target_link_options(yax86_picocalc PRIVATE -Wl,--print-memory-usage)

# Reports go out over USB CDC. GP0 and GP1, the UART's pins, are left alone.
pico_enable_stdio_usb(yax86_picocalc 1)
pico_enable_stdio_uart(yax86_picocalc 0)

# The LCD's serial interface - see src/lcd.c.
pico_generate_pio_header(yax86_picocalc "${CMAKE_CURRENT_SOURCE_DIR}/src/lcd.pio")
# The PSRAM's - see pico/common/psram.c.
pico_generate_pio_header(yax86_picocalc "${CMAKE_CURRENT_SOURCE_DIR}/../common/psram.pio")

pico_add_extra_outputs(yax86_picocalc)
