cmake_minimum_required(VERSION 3.14)
project(yax86)

# =============================================================================
# Main library
# =============================================================================
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB SOURCES "*.c")
add_library(yax86 STATIC ${SOURCES})

# =============================================================================
# Tests
# =============================================================================
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(tests)
endif()
