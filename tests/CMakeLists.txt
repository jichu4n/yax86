# =============================================================================
# GoogleTest
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

# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(all_tests ${TEST_SOURCES})
target_link_libraries(all_tests gtest_main yax86)

include(GoogleTest)
gtest_discover_tests(all_tests)

# =============================================================================
# Executable dependencies
# =============================================================================
find_program(FASM_EXECUTABLE fasm)
if(NOT FASM_EXECUTABLE)
  message(WARNING "fasm not found, tests will fail to run!")
endif()

find_program(OBJDUMP_EXECUTABLE objdump)
if(NOT OBJDUMP_EXECUTABLE)
  message(WARNING "objdump not found, tests will fail to run!")
endif()