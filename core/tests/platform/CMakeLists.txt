# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(platform_tests ${TEST_SOURCES})
target_link_libraries(platform_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(platform_tests)
