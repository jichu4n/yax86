# Project: yax86

## Project Goals

This project is a small modular IBM PC emulator for minimal MCU platforms like
the Raspberry Pi Pico.
- Emulates IBM PC, PC/XT, and PC/AT systems with 8086/8088 processors
- Runs MS-DOS 3.3

## Codebase

- The `src` directory contains the main emulator code
- Each directory in `src` is a module, bundled into a single header in the root
  directory based on `bundle.json`
    - For example, the source code in the CPU module `src/cpu` is bundled
      based on `src/cpu/bundle.json` into `cpu.h` in root directory
    - The external interface of each module is defined in `public.h`
    - The bundler is `tools/generate-header-bundle.js`
- The `tests` directory contains unit tests for each emulator module
- Uses CMake as the build system
    - The `build` directory contains build artifacts

## Coding Style

- Main emulator code is written in portable C99
- Tests are written in C++14 and use the Google Test framework
- All C/C++ code conforms to Google C++ style guide
- No dependencies on libc functions like `printf`
- No dynamic memory allocation - only uses compile-time static memory
  allocation and stack allocation
- OK to include standard library headers for types like `uint8_t` and
  compile-time constants and macros like `NULL`
- Uses `clang-format` for code formatting
- Prefer enums over `#define` for constants
- Prefer `static inline` functions over macros
- Prefer specific types like `uint8_t` over generic types like `int` for
  interfaces like function signatures and struct members
- Define structs and enums with `typedef struct Name { ... } Name;` or 
  `typedef enum Name { ... } Name;`

## Commands

To build the emulator from the project root directory:
```
cmake -B build && cmake --build build -j$(nproc)
```

To run tests:
```
ctest --test-dir build -j$(nproc) --output-on-failure
```

