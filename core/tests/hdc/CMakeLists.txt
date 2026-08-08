# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(hdc_tests ${TEST_SOURCES})
target_link_libraries(hdc_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(hdc_tests)
