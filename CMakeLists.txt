cmake_minimum_required(VERSION 3.14)
project(yax86 VERSION 1.0)

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
set(CMAKE_CXX_STANDARD 17)  # GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(tests)
