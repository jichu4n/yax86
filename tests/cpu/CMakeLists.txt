# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(all_tests ${TEST_SOURCES})
target_link_libraries(all_tests gtest_main yax86)

include(GoogleTest)
gtest_discover_tests(all_tests)

# =============================================================================
# Dependencies
# =============================================================================
find_program(FASM_EXECUTABLE fasm)
if(NOT FASM_EXECUTABLE)
  message(WARNING "fasm not found, tests will fail to run!")
endif()

find_program(OBJDUMP_EXECUTABLE objdump)
if(NOT OBJDUMP_EXECUTABLE)
  message(WARNING "objdump not found, tests will fail to run!")
endif()
