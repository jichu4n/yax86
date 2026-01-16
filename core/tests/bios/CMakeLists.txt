# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(bios_tests ${TEST_SOURCES})
target_link_libraries(bios_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(bios_tests)
