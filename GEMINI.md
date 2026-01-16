# Project: yax86

## Project Goals

This project is a small IBM PC/XT emulator for minimal MCU platforms like
the Raspberry Pi Pico, as well as the browser via SDL and Emscripten.
- Emulates a basic PC/XT system with an 8086/8088 processor
- Runs MS-DOS 3.3 on top of GLaBIOS

## Codebase

### core - main emulation logic

- The `core` directory contains the main emulator logic
- Each directory in `core/src` is a module, bundled into a single header in the root
  directory based on `bundle.json`
    - For example, the source code in the CPU module `core/src/cpu` is bundled
      based on `core/src/cpu/bundle.json` into `cpu.h` in `core` directory
    - The external interface of each module is defined in `public.h`
- The `core/tests` directory contains unit tests for each module
- Uses CMake as the build system
    - The `build` directory contains build artifacts

### web - WebAssembly + SDL port

- The `web` directory contains the WebAssembly + SDL port of the emulator
- Uses Emscripten as the toolchain and CMake as the build system


## Code Style

- Main emulator code is written in portable C99
- Tests are written in C++14 and use the Google Test framework
- All C/C++ code conforms to Google C++ style guide
- No dependencies on libc functions like `printf`, `memset`.
- No dynamic memory allocation - only uses compile-time static memory
  allocation and stack allocation
- For zero-initialized data, use static zero-initialization instead of
  `memset`, for example `static MyStruct data = {0};`
- OK to include standard library headers for types like `uint8_t` and
  compile-time constants and macros like `NULL`
- Uses `clang-format` for code formatting
- Prefer enums over `#define` for constants
- Prefer enums over numeric literals - define an enum value `enum { kFoo = 0xF8
  };` instead of referencing `0xF8` directly in the logic.
- Prefer `static inline` functions over macros
- Prefer specific types like `uint8_t` over generic types like `int` for
  interfaces like function signatures and struct members
- Define structs and enums with `typedef struct Name { ... } Name;` or 
  `typedef enum Name { ... } Name;`
- Unused function parameters should be annotated with `YAX86_UNUSED` from the
  util/common.h header to avoid unused parameter warnings
- When incrementing or decrementing a variable, prefer prefix syntax
  `++var` or `--var` instead of the suffix syntax `var++` or `var--`
- Comments should generally be added on the line before a variable, field or
  type, rather than on the same line. For example:
    ```c
    // Good comment - do this
    int var1;

    int var2;  // Bad comment - don't do this
    ```

## Commands

To build the emulator from the project root directory:
```
cmake -B build && cmake --build build -j8
```

To run tests:
```
ctest --test-dir build/core -j8 --output-on-failure
```

## Additional Notes

- The project should NOT implement emulation for features not used on the IBM
  PC/XT by GLaBIOS or MS-DOS. The project does not attempt to support other
  hypothetical x86 operating systems, only MS-DOS and era-accurate software.
    - For example, we should NOT implement advanced features of the Intel 8259A
      PIC like level-triggered interrupts, auto-EOI mode, or specific
      end-of-interrupt mode, because the IBM PC/XT only uses edge-triggered
      interrupts, manual EOI mode, and normal fully-nested mode.
- The project uses GlaBIOS as the BIOS implementation. The project SHOULD
  implement emulation for any functionality required by GlaBIOS to run MS-DOS
  3.3 and basic DOS applications.
- The source code for GLaBIOS is found at `.cache/GLaBIOS/src/GLABIOS.ASM`
  under the project root. Specifically, we target GLaBIOS's `ARCH_TYPE_EMU`
  build type.
- When considering how to implement emulation for a hardware component or a
  feature of a hardware component, fetch and review the source code of similar
  projects listed below for reference.

## Similar projects

The source code for the following similar projects can be found in the
`.cache` directory under the project root:

- 8086tiny - `.cache/8086tiny`
- 86Box - `.cache/86Box`
- MartyPC - `.cache/martypc`

