cmake_minimum_required(VERSION 3.14)

project(yax86_sdl)

if(EMSCRIPTEN)
    # Emscripten handles SDL3 via -s USE_SDL=3
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    target_link_options(SDL3::SDL3 INTERFACE "SHELL:-s USE_SDL=3")
else()
    find_package(SDL3 REQUIRED)
endif()

# =============================================================================
# Boot floppy image
# =============================================================================
# Copied to a space-free name so it can be passed to Emscripten's file packager
# and found next to the native binary.
set(BOOT_FLOPPY_SOURCE "${CMAKE_SOURCE_DIR}/resources/MS-DOS 3.30/DISK01.IMG")
set(BOOT_FLOPPY "${CMAKE_CURRENT_BINARY_DIR}/floppy_a.img")
add_custom_command(
    OUTPUT ${BOOT_FLOPPY}
    COMMAND ${CMAKE_COMMAND} -E copy "${BOOT_FLOPPY_SOURCE}" ${BOOT_FLOPPY}
    DEPENDS "${BOOT_FLOPPY_SOURCE}"
    COMMENT "Copying boot floppy image => ${BOOT_FLOPPY}"
)
add_custom_target(yax86_sdl_boot_floppy DEPENDS ${BOOT_FLOPPY})

add_executable(yax86_sdl
    src/main.c
    src/floppy.c
    src/display.c
    src/input.c
    src/audio.c
)
add_dependencies(yax86_sdl yax86_sdl_boot_floppy)

target_include_directories(yax86_sdl PRIVATE 
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(yax86_sdl PRIVATE yax86_core SDL3::SDL3)

if(EMSCRIPTEN)
    set_target_properties(yax86_sdl PROPERTIES SUFFIX ".html")
    target_compile_options(yax86_sdl PRIVATE "SHELL:-s USE_SDL=3")
    target_link_options(yax86_sdl PRIVATE
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s USE_SDL=3"
        # Package the boot floppy into the Emscripten filesystem, where
        # kDefaultFloppyImagePath expects to find it.
        "SHELL:--preload-file ${BOOT_FLOPPY}@/floppy_a.img"
    )
    set_property(TARGET yax86_sdl APPEND PROPERTY LINK_DEPENDS ${BOOT_FLOPPY})
endif()
