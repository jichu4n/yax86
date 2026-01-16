# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(fdc_tests ${TEST_SOURCES})
target_link_libraries(fdc_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(fdc_tests)
