# Project: yax86

## Project Goals

This project is a small IBM PC/XT emulator for minimal MCU platforms like
the Raspberry Pi Pico, as well as the browser via SDL and Emscripten.
- Emulates a basic PC/XT system with an 8086/8088 processor
- Runs MS-DOS 3.3 on top of GLaBIOS

## Codebase

### core - core emulation logic

- The `core` directory contains the core emulation logic, including modules
  that emulate the CPU and hardware such as video adapter, keyboard and floppy.
- Importantly, `core` modules do NOT depend on any external runtime (SDL etc)
  and only interact with the host via callbacks.
- Each directory under `core/src` corresponds to a module. Each module bundled
  into a single header in the root directory based on `bundle.json`.
    - For example, the source code in the CPU module `core/src/cpu` is bundled
      based on `core/src/cpu/bundle.json` into `cpu.h` in `core` directory
    - The external interface of each module is defined in its `public.h`.
- The `platform` module in `core/src/platform` is the "virtual motherboard"
  that connects the other modules together, including memory and port mapping.
- The `core/tests` directory contains unit tests for each module.
- The output of the build system for `core` is a static library
  `libyax86_core.a`.

### sdl - SDL runtime

- The `sdl` directory contains an SDL3-based runtime for the emulator.
- It is compiled via Emscripten to produce a WebAssembly binary and JavaScript
  wrapper (`yax86_sdl.{wasm,js}`).

## Code Style

- Core emulator code is written in portable C99.
- Tests are written in C++14 and use the Google Test framework.
- All C/C++ code conforms to Google C++ style guide.
- No dependencies on libc functions like `printf`, `memset`.
- No dynamic memory allocation - only uses compile-time static memory
  allocation and stack allocation.
- For zero-initialized data, use static zero-initialization instead of
  `memset`, for example `static MyStruct data = {0};`.
- OK to include standard library headers for types like `uint8_t` and
  compile-time constants and macros like `NULL`.
- Uses `clang-format` for code formatting.
- Prefer enums over `#define` for constants.
- Prefer enums over numeric literals - define an enum value `enum { kFoo = 0xF8
  };` instead of referencing `0xF8` directly in the logic.
- Prefer `static inline` functions over macros.
- Prefer specific types like `uint8_t` over generic types like `int` for
  interfaces like function signatures and struct members.
- Define structs and enums with `typedef struct Name { ... } Name;` or 
  `typedef enum Name { ... } Name;`.
- Unused function parameters should be annotated with `YAX86_UNUSED` from the
  util/common.h header to avoid unused parameter warnings.
- When incrementing or decrementing a variable, prefer prefix syntax
  `++var` or `--var` instead of the suffix syntax `var++` or `var--`.
- Comments should generally be added on the line before a variable, field or
  type, rather than on the same line. For example:
    ```c
    // Good comment - do this
    int var1;

    int var2;  // Bad comment - don't do this
    ```

## Commands

The project uses CMake as the build system.
- The `build-native` directory contains output of native builds - native static
  libraries, test binaries and executables.
- The `build-emscripten` directory contains output of Emscripten builds -
  WebAssembly binary and JavaScript wrapper.

To build the emulator from the project root directory:
```
./tools/build.sh
```

To run tests:
```
ctest --test-dir build-native/core -j$(nproc) --output-on-failure
```

To debug the WASM version of the emulator, use Chrome DevTools MCP server to
open `http://localhost:3000/yax86_sdl.html`.

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

