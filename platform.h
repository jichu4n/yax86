// ==============================================================================
// YAX86 PLATFORM MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PLATFORM_BUNDLE_H
#define YAX86_PLATFORM_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)          \
  typedef struct name {                                                   \
    StaticVectorHeader header;                                            \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const StaticVectorHeader header = {                            \
        .element_size = sizeof(element_type),                             \
        .max_length = (max_length_value),                                 \
        .length = 0,                                                      \
    };                                                                    \
    vector->header = header;                                              \
  }                                                                       \
  static size_t name##Length(const name* vector) __attribute__((unused)); \
  static size_t name##Length(const name* vector) {                        \
    return vector->header.length;                                         \
  }                                                                       \
  static element_type* name##Get(name* vector, size_t index)              \
      __attribute__((unused));                                            \
  static element_type* name##Get(name* vector, size_t index) {            \
    if (index >= (max_length_value)) {                                    \
      return NULL;                                                        \
    }                                                                     \
    return &(vector->elements[index]);                                    \
  }                                                                       \
  static bool name##Append(name* vector, const element_type* element)     \
      __attribute__((unused));                                            \
  static bool name##Append(name* vector, const element_type* element) {   \
    if (vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    vector->elements[vector->header.length++] = *element;                 \
    return true;                                                          \
  }                                                                       \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element)            \
      __attribute__((unused));                                            \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element) {          \
    if (index > vector->header.length ||                                  \
        vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    for (size_t i = vector->header.length; i > index; --i) {              \
      vector->elements[i] = vector->elements[i - 1];                      \
    }                                                                     \
    vector->elements[index] = *element;                                   \
    ++vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static bool name##Remove(name* vector, size_t index)                    \
      __attribute__((unused));                                            \
  static bool name##Remove(name* vector, size_t index) {                  \
    if (index >= vector->header.length) {                                 \
      return false;                                                       \
    }                                                                     \
    for (size_t i = index; i < vector->header.length - 1; ++i) {          \
      vector->elements[i] = vector->elements[i + 1];                      \
    }                                                                     \
    --vector->header.length;                                              \
    return true;                                                          \
  }

#endif  // YAX86_UTIL_STATIC_VECTOR_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================

// ==============================================================================
// src/platform/public.h start
// ==============================================================================

#line 1 "./src/platform/public.h"
// Public interface for the Platform module.
#ifndef YAX86_PLATFORM_PUBLIC_H
#define YAX86_PLATFORM_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_PLATFORM_BUNDLE_H
#include "../util/static_vector.h"
#endif  // YAX86_PLATFORM_BUNDLE_H

#include "cpu.h"

struct PlatformState;

// ============================================================================
// Memory mapping
// ============================================================================

// Type ID of a memory map entry.
typedef uint8_t MemoryMapEntryType;

enum {
  // Conventional memory - first 640KB of physical memory, mapped to 0x00000 to
  // 0x9FFFF (640KB).
  kMemoryMapEntryConventional = 0,

  // Maximum number of memory map entries.
  kMaxMemoryMapEntries = 16,
};

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct MemoryMapEntry {
  // The memory map entry type, such as kMemoryMapEntryConventional.
  MemoryMapEntryType entry_type;
  // Start address of the memory map entry.
  uint32_t start;
  // Size of the memory map entry in bytes.
  uint32_t size;
  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry.
  uint8_t (*read_byte)(
      struct PlatformState* platform, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address.
  void (*write_byte)(
      struct PlatformState* platform, uint32_t relative_address, uint8_t value);
} MemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    struct PlatformState* platform, const MemoryMapEntry* entry);
// Look up the memory map entry corresponding to an address. Returns NULL if the
// address is not mapped to a known memory map entry.
MemoryMapEntry* GetMemoryMapEntryForAddress(
    struct PlatformState* platform, uint32_t address);
// Look up a memory map entry by type. Returns NULL if no entry found with the
// specified type.
MemoryMapEntry* GetMemoryMapEntryByType(
    struct PlatformState* platform, MemoryMapEntryType entry_type);

// Read a byte from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
uint8_t ReadMemoryByte(struct PlatformState* platform, uint32_t address);
// Read a word from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
uint16_t ReadMemoryWord(struct PlatformState* platform, uint32_t address);
// Write a byte to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
void WriteMemoryByte(
    struct PlatformState* platform, uint32_t address, uint8_t value);
// Write a word to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
void WriteMemoryWord(
    struct PlatformState* platform, uint32_t address, uint16_t value);

// ============================================================================
// I/O port mapping
// ============================================================================

// Type ID of an I/O port map entry.
typedef uint8_t IOPortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  kMaxIOPortMapEntries = 16,
};

// An I/O port map entry. Entries should not overlap.
typedef struct IOPortMapEntry {
  // The I/O port map entry type, such as kIOPortMapEntryConventional.
  IOPortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Number of ports in the I/O port range.
  uint16_t size;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct PlatformState* platform, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(
      struct PlatformState* platform, uint16_t port, uint8_t value);
} IOPortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterIOPortMapEntry(
    struct PlatformState* platform, const IOPortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
IOPortMapEntry* GetIOPortMapEntryForPort(
    struct PlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
IOPortMapEntry* GetIOPortMapEntryByType(
    struct PlatformState* platform, IOPortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadIOPortByte(struct PlatformState* platform, uint16_t port);
// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadIOPortWord(struct PlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WriteIOPortByte(
    struct PlatformState* platform, uint16_t port, uint8_t value);
// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WriteIOPortWord(
    struct PlatformState* platform, uint16_t port, uint16_t value);

// ============================================================================
// Platform state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct PlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Physical memory size in KB (1024 bytes). Must be between 64 and 640.
  uint16_t physical_memory_size_kb;

  // Callback to read a byte from physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address);

  // Callback to write a byte to physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address, uint8_t value);
} PlatformConfig;

STATIC_VECTOR_TYPE(MemoryMap, MemoryMapEntry, kMaxMemoryMapEntries)
STATIC_VECTOR_TYPE(IOPortMap, IOPortMapEntry, kMaxIOPortMapEntries)

// State of the platform.
typedef struct PlatformState {
  // Pointer to caller-provided runtime configuration
  PlatformConfig* config;
  // Memory map.
  MemoryMap memory_map;
  // I/O port map.
  IOPortMap io_port_map;
} PlatformState;

#endif  // YAX86_PLATFORM_PUBLIC_H


// ==============================================================================
// src/platform/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/util/common.h start
// ==============================================================================

#line 1 "./src/util/common.h"
#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Macro to mark a function or parameter as unused.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_UNUSED __attribute__((unused))
#else
#define YAX86_UNUSED
#endif  // defined(__GNUC__) || defined(__clang__)

#endif  // YAX86_UTIL_COMMON_H


// ==============================================================================
// src/util/common.h end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PLATFORM_BUNDLE_H

