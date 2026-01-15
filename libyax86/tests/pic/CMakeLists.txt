# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(pic_tests ${TEST_SOURCES})
target_link_libraries(pic_tests gtest_main yax86)

include(GoogleTest)
gtest_discover_tests(pic_tests)
