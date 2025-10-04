# Project: yax86

## Project Goals

This project is a small IBM PC emulator for minimal MCU platforms like
the Raspberry Pi Pico.
- Emulates a basic IBM PC, PC/XT, and PC/AT system with an 8086/8088 processor
- Runs MS-DOS 3.3 on top of GLaBIOS

## Codebase

- The `src` directory contains the main emulator code
- Each directory in `src` is a module, bundled into a single header in the root
  directory based on `bundle.json`
    - For example, the source code in the CPU module `src/cpu` is bundled
      based on `src/cpu/bundle.json` into `cpu.h` in root directory
    - The external interface of each module is defined in `public.h`
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
- Prefer enums over numeric literals - create `enum { kFoo = 0xF8 };` instead of
  referencing `0xF8` directly in the logic.
- Prefer `static inline` functions over macros
- Prefer specific types like `uint8_t` over generic types like `int` for
  interfaces like function signatures and struct members
- Define structs and enums with `typedef struct Name { ... } Name;` or 
  `typedef enum Name { ... } Name;`
- For unused function parameters, use `__attribute__((unused))` to avoid
  compiler warnings

## Commands

To build the emulator from the project root directory:
```
cmake -B build && cmake --build build -j8
```

To run tests:
```
ctest --test-dir build -j8 --output-on-failure
```

## Additional Notes

- The project should NOT implement emulation for features not used on the IBM
  PC, PC/XT, or PC/AT systems by the IBM BIOS or MS-DOS. The project does not
  attempt to support other hypothetical x86 operating systems, only MS-DOS and
  era-accurate software.
    - For example, we should NOT implement advanced features of the Intel
      8259A PIC like level-triggered interrupts, auto-EOI mode, or specific
      end-of-interrupt mode, because the IBM PC/XT and PC/AT systems only use
      edge-triggered interrupts, manual EOI mode, and normal fully-nested mode.
- The project uses GlaBIOS as the BIOS implementation. The project SHOULD
  implement emulation for any functionality required by GlaBIOS to run MS-DOS
  3.3 and basic DOS applications.
- The source code for GLaBIOS is found at `.cache/GLaBIOS` under the project
  root.
- When considering how to implement emulation for a hardware component or a
  feature of a hardware component, fetch and review the source code of similar
  projects listed below for reference.

## Similar projects

- 8086tiny - https://github.com/adriancable/8086tiny
- pico-xt - https://github.com/xrip/pico-xt
- picox86 - https://github.com/mathijsvandenberg/picox86
- XTulator - https://github.com/mikechambers84/XTulator
- Faux86 - https://github.com/jhhoward/Faux86

For each project, the source code can be found in the `.cache` directory under
the project root.

