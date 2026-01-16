# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(keyboard_test ${TEST_SOURCES})
target_link_libraries(keyboard_test gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(keyboard_test)
