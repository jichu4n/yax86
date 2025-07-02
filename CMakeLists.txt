cmake_minimum_required(VERSION 3.14)
project(yax86)

option(ENABLE_SDL "Enable the SDL3-based user interface" OFF)

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
    file(GLOB common_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
    add_custom_command(
        OUTPUT ${bundle}
        COMMAND ${generate_header_bundle} ARGS ${module}
        DEPENDS ${generate_header_bundle} ${module_sources} ${common_sources}
        COMMENT "Generating header bundle for module ${module} => ${bundle}"
    )
    set(${module}_BUNDLE "${bundle}" PARENT_SCOPE)
    list(APPEND ALL_HEADER_BUNDLES ${bundle})
    set(ALL_HEADER_BUNDLES ${ALL_HEADER_BUNDLES} PARENT_SCOPE)
endfunction()

generate_header_bundle("cpu")
generate_header_bundle("bios_sdl")

# =============================================================================
# Main library
# =============================================================================
add_library(yax86 STATIC all.c ${ALL_HEADER_BUNDLES})

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
    add_subdirectory(tests)
endif()

# =============================================================================
# Demo programs
# =============================================================================
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    add_subdirectory(demos/cpu)
endif()
