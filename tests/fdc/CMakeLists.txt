add_executable(fdc_test fdc_test.cpp)
target_link_libraries(fdc_test PRIVATE yax86 gtest_main)
add_test(NAME fdc_test COMMAND fdc_test)
