# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(video_tests ${TEST_SOURCES})
target_link_libraries(video_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(video_tests)
