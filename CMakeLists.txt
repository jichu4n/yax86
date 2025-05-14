cmake_minimum_required(VERSION 3.10)

# Project name
project(yax86 LANGUAGES C)

# Add the static library
add_library(yax86 STATIC yax86.c)

# Disable standard library dependencies
set_target_properties(yax86 PROPERTIES 
    COMPILE_FLAGS "-nostdlib -fno-builtin"
    LINK_FLAGS "-nostdlib")

# Install targets
install(TARGETS yax86
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)