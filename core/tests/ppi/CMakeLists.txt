# =============================================================================
# Tests
# =============================================================================
file(GLOB TEST_SOURCES "*.cpp")
add_executable(ppi_tests ${TEST_SOURCES})
target_link_libraries(ppi_tests gtest_main yax86_core)

include(GoogleTest)
gtest_discover_tests(ppi_tests)
