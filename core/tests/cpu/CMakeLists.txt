# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
# The hardware test suite is built separately, below.
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/hardware_test.cpp")
add_executable(cpu_tests ${TEST_SOURCES})
target_link_libraries(cpu_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(cpu_tests)

# =============================================================================
# 8088 CPU hardware test suite
# =============================================================================
# Deliberately not registered with ctest: the set of tests depends on which
# opcodes have been downloaded, which ctest would bake in at build time.
# tools/run-tests.sh runs the binary directly.
#
# zlib is required rather than optional. The suite ships compressed, and
# building it conditionally would mean tools/run-tests.sh had to cope with the
# binary not being there - which is the sort of thing that ends up reporting
# success while checking nothing.
find_package(ZLIB REQUIRED)
add_executable(cpu_hardware_tests hardware_test.cpp)
target_link_libraries(cpu_hardware_tests gtest_main yax86_core ZLIB::ZLIB)

# Location of the test data. Opcodes are enumerated when the tests run, so
# downloading more of them does not need a rebuild.
target_compile_definitions(cpu_hardware_tests PRIVATE
  YAX86_CPU_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/.cache/8088-tests")

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
