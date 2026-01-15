file(GLOB TEST_SOURCES "*.cpp")
add_executable(dma_tests ${TEST_SOURCES})
target_link_libraries(dma_tests gtest_main yax86)

include(GoogleTest)
gtest_discover_tests(dma_tests)
