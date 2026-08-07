# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(util_tests ${TEST_SOURCES})
target_link_libraries(util_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(util_tests)
