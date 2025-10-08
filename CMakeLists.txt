cmake_minimum_required(VERSION 3.14)
project(yax86)

option(ENABLE_SDL "Enable the SDL3-based user interface" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -Werror")

# =============================================================================
# Header bundles.
# =============================================================================
set(ALL_HEADER_BUNDLES)
function(generate_header_bundle module)
    set(bundle "${CMAKE_CURRENT_SOURCE_DIR}/${module}.h")
    set(generate_header_bundle "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-header-bundle.js")
    file(GLOB module_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}/*")
    file(GLOB util_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/util/*")
    add_custom_command(
        OUTPUT ${bundle}
        COMMAND ${generate_header_bundle} ARGS ${module}
        DEPENDS ${generate_header_bundle} ${module_sources} ${util_sources}
        COMMENT "Generating header bundle for module ${module} => ${bundle}"
    )
    set(${module}_BUNDLE "${bundle}" PARENT_SCOPE)
    list(APPEND ALL_HEADER_BUNDLES ${bundle})
    set(ALL_HEADER_BUNDLES ${ALL_HEADER_BUNDLES} PARENT_SCOPE)
endfunction()

generate_header_bundle("bios")
generate_header_bundle("cpu")
generate_header_bundle("pic")
generate_header_bundle("pit")
generate_header_bundle("ppi")
generate_header_bundle("keyboard")
generate_header_bundle("dma")
generate_header_bundle("platform")
generate_header_bundle("video")

# =============================================================================
# BIOS ROM data file generation
# =============================================================================
set(bios_rom_file "${CMAKE_CURRENT_SOURCE_DIR}/src/bios/GLABIOS_0.2.5_8E.rom")
set(bios_rom_data_file_prefix "${CMAKE_CURRENT_SOURCE_DIR}/src/bios/bios_rom_data")
set(bios_rom_data_file_c "${bios_rom_data_file_prefix}.c")
set(bios_rom_data_file_h "${bios_rom_data_file_prefix}.h")
set(generate_bios_rom_data_files "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-bios-rom-data-files.js")
add_custom_command(
    OUTPUT ${bios_rom_data_file_c} ${bios_rom_data_file_h}
    COMMAND ${generate_bios_rom_data_files} ARGS ${bios_rom_file} ${bios_rom_data_file_prefix}
    DEPENDS ${generate_bios_rom_data_files} ${bios_rom_file}
    COMMENT "Generating BIOS ROM data file ${bios_rom_file} => ${bios_rom_data_file_c} and ${bios_rom_data_file_h}"
)

# =============================================================================
# Main library
# =============================================================================
add_library(yax86 STATIC all.c ${ALL_HEADER_BUNDLES} ${bios_rom_data_file_c})

# =============================================================================
# SDL
# =============================================================================
if(ENABLE_SDL)
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.16
    )
    FetchContent_MakeAvailable(SDL3)
    target_link_libraries(yax86 PRIVATE SDL3::SDL3)
endif()

# =============================================================================
# Tests
# =============================================================================
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    add_subdirectory(tests)
endif()

# =============================================================================
# Demo programs
# =============================================================================
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    add_subdirectory(tools/cpu_demo)
endif()

