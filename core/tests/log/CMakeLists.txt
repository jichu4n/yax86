# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(log_tests ${TEST_SOURCES})
target_link_libraries(log_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(log_tests)
