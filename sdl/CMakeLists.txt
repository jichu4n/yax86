cmake_minimum_required(VERSION 3.14)

project(yax86_sdl)

if(EMSCRIPTEN)
    # Emscripten handles SDL3 via -s USE_SDL=3
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    target_link_options(SDL3::SDL3 INTERFACE "SHELL:-s USE_SDL=3")
else()
    find_package(SDL3 REQUIRED)
endif()

add_executable(yax86_sdl
    src/main.c
    src/display.c
    src/input.c
)

target_include_directories(yax86_sdl PRIVATE 
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(yax86_sdl PRIVATE yax86_core SDL3::SDL3)

if(EMSCRIPTEN)
    set_target_properties(yax86_sdl PROPERTIES SUFFIX ".html")
    target_compile_options(yax86_sdl PRIVATE "SHELL:-s USE_SDL=3")
    target_link_options(yax86_sdl PRIVATE
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s USE_SDL=3"
    )
endif()
