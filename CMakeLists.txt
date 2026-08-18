cmake_minimum_required(VERSION 3.14)
project(yax86)

# Default to Release build. Necessary as there's a large performance gap (~4x)
# between -O2/-O3 and -O0 mainly due to inlining.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -Werror")

add_subdirectory(core)
add_subdirectory(sdl)

