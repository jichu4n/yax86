# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(pit_tests ${TEST_SOURCES})
target_link_libraries(pit_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(pit_tests)
