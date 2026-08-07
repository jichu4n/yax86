# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(cpu_tests ${TEST_SOURCES})
target_link_libraries(cpu_tests gtest_main yax86_core)

# Location of the 8088 hardware test suite data, fetched on demand by
# tools/download-cpu-tests.sh. Tests are skipped when nothing is downloaded.
target_compile_definitions(cpu_tests PRIVATE
  YAX86_CPU_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/.cache/8088-tests")

include(GoogleTest)
gtest_discover_tests(cpu_tests)

# =============================================================================
# Dependencies
# =============================================================================
find_program(NASM_EXECUTABLE nasm)
if(NOT NASM_EXECUTABLE)
  message(WARNING "nasm not found, tests will fail to run!")
endif()

find_program(NDISASM_EXECUTABLE ndisasm)
if(NOT NDISASM_EXECUTABLE)
  message(WARNING "ndisasm not found, tests will fail to run!")
endif()
