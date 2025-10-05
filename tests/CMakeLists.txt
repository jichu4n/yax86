# =============================================================================
# GoogleTest
# =============================================================================
set(CMAKE_CXX_STANDARD 17) # GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# =============================================================================
# Tests for each module
# =============================================================================

add_subdirectory(cpu)
add_subdirectory(pic)
add_subdirectory(pit)
# add_subdirectory(bios)
