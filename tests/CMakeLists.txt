file(GLOB TEST_SOURCES "*.cpp")

add_executable(all_tests ${TEST_SOURCES})

target_link_libraries(all_tests gtest_main yax86)

include(GoogleTest)
gtest_discover_tests(all_tests)