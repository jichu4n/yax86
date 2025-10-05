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
#include "pic.h"
#include "pit.h"

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

  // Maximum size of physical memory in bytes.
  kMaxPhysicalMemorySize = 640 * 1024,
  // Minimum size of physical memory in bytes.
  kMinPhysicalMemorySize = 64 * 1024,
};

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct MemoryMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The memory map entry type, such as kMemoryMapEntryConventional.
  MemoryMapEntryType entry_type;
  // Start address of the memory region.
  uint32_t start;
  // Inclusive end address of the memory region.
  uint32_t end;
  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry.
  uint8_t (*read_byte)(struct MemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address.
  void (*write_byte)(
      struct MemoryMapEntry* entry, uint32_t relative_address, uint8_t value);
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
typedef uint8_t PortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  kMaxPortMapEntries = 16,
  // I/O port map entry for the master PIC (ports 0x20-0x21).
  kPortMapEntryPICMaster = 0x20,
  // I/O port map entry for the slave PIC (ports 0xA0-0xA1).
  kPortMapEntryPICSlave = 0xA0,
  // I/O port map entry for the PIT (ports 0x40-0x43).
  kPortMapEntryPIT = 0x40,
};

// An I/O port map entry. Entries should not overlap.
typedef struct PortMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The I/O port map entry type.
  PortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Inclusive end of the I/O port range.
  uint16_t end;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct PortMapEntry* entry, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(struct PortMapEntry* entry, uint16_t port, uint8_t value);
} PortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterPortMapEntry(
    struct PlatformState* platform, const PortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
PortMapEntry* GetPortMapEntryForPort(
    struct PlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
PortMapEntry* GetPortMapEntryByType(
    struct PlatformState* platform, PortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadPortByte(struct PlatformState* platform, uint16_t port);
// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadPortWord(struct PlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WritePortByte(
    struct PlatformState* platform, uint16_t port, uint8_t value);
// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WritePortWord(
    struct PlatformState* platform, uint16_t port, uint16_t value);

// ============================================================================
// Platform state
// ============================================================================

// PIC configuration.
typedef enum PlatformPICMode {
  // Single PIC - IBM PC, PC/XT.
  kPlatformPICModeSingle,
  // Dual PIC (master and slave) - IBM PC/AT, PS/2.
  kPlatformPICModeDual,
} PlatformPICMode;

// Caller-provided runtime configuration.
typedef struct PlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Physical memory size in bytes. Must be between 64K and 640K.
  uint32_t physical_memory_size;

  // PIC configuration.
  PlatformPICMode pic_mode;

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
STATIC_VECTOR_TYPE(PortMap, PortMapEntry, kMaxPortMapEntries)

// State of the platform.
typedef struct PlatformState {
  // Pointer to caller-provided runtime configuration.
  PlatformConfig* config;

  // CPU runtime configuration.
  CPUConfig cpu_config;
  // CPU state.
  CPUState cpu;

  // Master PIC runtime configuration.
  PICConfig master_pic_config;
  // Master PIC state.
  PICState master_pic;

  // Slave PIC runtime configuration. Only valid if pic_mode is
  // kPlatformPICModeDual.
  PICConfig slave_pic_config;
  // Slave PIC state. Only valid if pic_mode is kPlatformPICModeDual.
  PICState slave_pic;

  // PIT runtime configuration.
  PITConfig pit_config;
  // PIT state.
  PITState pit;

  // Memory map.
  MemoryMap memory_map;
  // I/O port map.
  PortMap io_port_map;
} PlatformState;

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config);

// Raise a hardware interrupt to the CPU via the PIC(s). Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq);

// Boot the virtual machine and start execution.
ExecuteStatus PlatformBoot(PlatformState* platform);

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

// ==============================================================================
// src/platform/platform.c start
// ==============================================================================

#line 1 "./src/platform/platform.c"
#include "pic.h"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    PlatformState* platform, const MemoryMapEntry* entry) {
  if (MemoryMapLength(&platform->memory_map) >= kMaxMemoryMapEntries) {
    return false;
  }
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* existing_entry = MemoryMapGet(&platform->memory_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return MemoryMapAppend(&platform->memory_map, entry);
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryMapEntry* GetMemoryMapEntryForAddress(
    PlatformState* platform, uint32_t address) {
  // TODO: Use a more efficient data structure for lookups, such as a sorted
  // array with binary search.
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* entry = MemoryMapGet(&platform->memory_map, i);
    if (address >= entry->start && address <= entry->end) {
      return entry;
    }
  }
  return NULL;
}

// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
MemoryMapEntry* GetMemoryMapEntryByType(
    PlatformState* platform, uint8_t entry_type) {
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* entry = MemoryMapGet(&platform->memory_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
uint8_t ReadMemoryByte(PlatformState* platform, uint32_t address) {
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->read_byte) {
    return 0xFF;
  }
  return entry->read_byte(entry, address - entry->start);
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(PlatformState* platform, uint32_t address) {
  uint8_t low_byte = ReadMemoryByte(platform, address);
  uint8_t high_byte = ReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteMemoryByte(PlatformState* platform, uint32_t address, uint8_t value) {
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->write_byte) {
    return;
  }
  entry->write_byte(entry, address - entry->start, value);
}

// Write a word to a logical memory address.
void WriteMemoryWord(
    PlatformState* platform, uint32_t address, uint16_t value) {
  WriteMemoryByte(platform, address, value & 0xFF);
  WriteMemoryByte(platform, address + 1, (value >> 8) & 0xFF);
}

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterPortMapEntry(PlatformState* platform, const PortMapEntry* entry) {
  if (PortMapLength(&platform->io_port_map) >= kMaxPortMapEntries) {
    return false;
  }
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* existing_entry = PortMapGet(&platform->io_port_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return PortMapAppend(&platform->io_port_map, entry);
}

// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
PortMapEntry* GetPortMapEntryForPort(PlatformState* platform, uint16_t port) {
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* entry = PortMapGet(&platform->io_port_map, i);
    if (port >= entry->start && port <= entry->end) {
      return entry;
    }
  }
  return NULL;
}
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
PortMapEntry* GetPortMapEntryByType(
    PlatformState* platform, PortMapEntryType entry_type) {
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* entry = PortMapGet(&platform->io_port_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadPortByte(PlatformState* platform, uint16_t port) {
  PortMapEntry* entry = GetPortMapEntryForPort(platform, port);
  if (!entry || !entry->read_byte) {
    return 0xFF;
  }
  return entry->read_byte(entry, port);
}

// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadPortWord(PlatformState* platform, uint16_t port) {
  uint8_t low_byte = ReadPortByte(platform, port);
  uint8_t high_byte = ReadPortByte(platform, port + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WritePortByte(PlatformState* platform, uint16_t port, uint8_t value) {
  PortMapEntry* entry = GetPortMapEntryForPort(platform, port);
  if (!entry || !entry->write_byte) {
    return;
  }
  entry->write_byte(entry, port, value);
}

// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WritePortWord(PlatformState* platform, uint16_t port, uint16_t value) {
  WritePortByte(platform, port, value & 0xFF);
  WritePortByte(platform, port + 1, (value >> 8) & 0xFF);
}

static uint8_t CPUReadMemoryByte(CPUState* cpu, uint32_t address) {
  return ReadMemoryByte((PlatformState*)cpu->config->context, address);
}

static void CPUWriteMemoryByte(CPUState* cpu, uint32_t address, uint8_t value) {
  WriteMemoryByte((PlatformState*)cpu->config->context, address, value);
}

static uint8_t CPUReadPortByte(CPUState* cpu, uint16_t port) {
  return ReadPortByte((PlatformState*)cpu->config->context, port);
}

static void CPUWritePortByte(CPUState* cpu, uint16_t port, uint8_t value) {
  WritePortByte((PlatformState*)cpu->config->context, port, value);
}

// Callback for the CPU to check for pending interrupts from the PIC after an
// instruction has been executed. This is how we connect the PIC(s) to the
// CPU's interrupt handling flow.
static ExecuteStatus CPUOnAfterExecuteInstruction(
    CPUState* cpu, YAX86_UNUSED const struct Instruction* instruction) {
  PlatformState* platform = (PlatformState*)cpu->config->context;

  if (!GetFlag(cpu, kIF)) {
    return kExecuteSuccess;
  }

  uint8_t interrupt_vector = PICGetPendingInterrupt(&platform->master_pic);
  if (interrupt_vector != kPICNoPendingInterrupt) {
    SetPendingInterrupt(cpu, interrupt_vector);
  }

  return kExecuteSuccess;
}

static const CPUConfig kEmptyCPUConfig = {0};

static uint8_t ReadPhysicalMemoryByte(MemoryMapEntry* entry, uint32_t address) {
  PlatformState* platform = (PlatformState*)entry->context;
  if (platform->config && platform->config->read_physical_memory_byte) {
    return platform->config->read_physical_memory_byte(platform, address);
  }
  return 0xFF;
}

static void WritePhysicalMemoryByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  PlatformState* platform = (PlatformState*)entry->context;
  if (platform->config && platform->config->write_physical_memory_byte) {
    platform->config->write_physical_memory_byte(platform, address, value);
  }
}

static uint8_t PICReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PICReadPort((PICState*)entry->context, port);
}

static void PICWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PICWritePort((PICState*)entry->context, port, value);
}

void PlatformRaiseIRQ0(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 0);
}

static uint8_t PITReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PITReadPort((PITState*)entry->context, port);
}

static void PITWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PITWritePort((PITState*)entry->context, port, value);
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config) {
  if (config->physical_memory_size < kMinPhysicalMemorySize ||
      config->physical_memory_size > kMaxPhysicalMemorySize) {
    return false;
  }

  platform->config = config;

  // Set up CPU.
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.read_memory_byte = CPUReadMemoryByte;
  platform->cpu_config.write_memory_byte = CPUWriteMemoryByte;
  platform->cpu_config.read_port = CPUReadPortByte;
  platform->cpu_config.write_port = CPUWritePortByte;
  platform->cpu_config.on_after_execute_instruction =
      CPUOnAfterExecuteInstruction;
  CPUInit(&platform->cpu, &platform->cpu_config);

  // Set up initial memory map.
  MemoryMapInit(&platform->memory_map);
  MemoryMapEntry conventional_memory = {
      .context = platform,
      .entry_type = kMemoryMapEntryConventional,
      .start = 0x0000,
      .end = config->physical_memory_size - 1,
      .read_byte = ReadPhysicalMemoryByte,
      .write_byte = WritePhysicalMemoryByte};
  MemoryMapAppend(&platform->memory_map, &conventional_memory);

  // Set up master PIC.
  platform->master_pic_config.sp = false;
  PICInit(&platform->master_pic, &platform->master_pic_config);
  PortMapEntry master_pic_entry = {
      .entry_type = kPortMapEntryPICMaster,
      .start = 0x20,
      .end = 0x21,
      .read_byte = PICReadPortByte,
      .write_byte = PICWritePortByte,
      .context = &platform->master_pic,
  };
  RegisterPortMapEntry(platform, &master_pic_entry);

  // Set up slave PIC if in dual PIC mode.
  if (config->pic_mode == kPlatformPICModeDual) {
    platform->slave_pic_config.sp = true;
    PICInit(&platform->slave_pic, &platform->slave_pic_config);
    platform->master_pic.cascade_pic = &platform->slave_pic;
    platform->slave_pic.cascade_pic = &platform->master_pic;
    PortMapEntry slave_pic_entry = {
        .entry_type = kPortMapEntryPICSlave,
        .start = 0xA0,
        .end = 0xA1,
        .read_byte = PICReadPortByte,
        .write_byte = PICWritePortByte,
        .context = &platform->slave_pic,
    };
    RegisterPortMapEntry(platform, &slave_pic_entry);
  }

  // Set up PIT.
  platform->pit_config.context = platform;
  platform->pit_config.raise_irq_0 = PlatformRaiseIRQ0;
  platform->pit_config.set_pc_speaker_frequency = NULL;  // TODO
  PITInit(&platform->pit, &platform->pit_config);
  PortMapEntry pit_entry = {
      .entry_type = kPortMapEntryPIT,
      .start = 0x40,
      .end = 0x43,
      .read_byte = PITReadPortByte,
      .write_byte = PITWritePortByte,
      .context = &platform->pit,
  };
  RegisterPortMapEntry(platform, &pit_entry);

  return true;
}

bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq) {
  switch (platform->config->pic_mode) {
    case kPlatformPICModeSingle:
      if (irq >= 8) {
        return false;
      }
      PICRaiseIRQ(&platform->master_pic, irq);
      return true;
    case kPlatformPICModeDual: {
      if (irq >= 16) {
        return false;
      }
      PICState* target_pic =
          (irq < 8) ? &platform->master_pic : &platform->slave_pic;
      PICRaiseIRQ(target_pic, irq % 8);
      return true;
    }
    default:
      return false;
  }
}

// Boot the virtual machine and start execution.
ExecuteStatus PlatformBoot(PlatformState* platform) {
  // Initialize CPU registers.
  // CS:IP points to the BIOS entry point at 0xFFFF0.
  platform->cpu.registers[kCS] = 0xF000;
  platform->cpu.registers[kIP] = 0xFFF0;
  platform->cpu.registers[kDS] = 0x0000;
  platform->cpu.registers[kSS] = 0x0000;
  platform->cpu.registers[kES] = 0x0000;
  platform->cpu.registers[kSP] = 0xFFFE;

  return RunMainLoop(&platform->cpu);
}


// ==============================================================================
// src/platform/platform.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PLATFORM_BUNDLE_H

