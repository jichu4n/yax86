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
  return entry->read_byte(platform, address - entry->start);
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(PlatformState* platform, uint32_t address) {
  uint8_t low_byte = ReadMemoryByte(platform, address);
  uint8_t high_byte = ReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteMemoryByte(PlatformState* platform, uint32_t address, uint8_t value) {
  MemoryMapEntry* region = GetMemoryMapEntryForAddress(platform, address);
  if (!region || !region->write_byte) {
    return;
  }
  region->write_byte(platform, address - region->start, value);
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
bool RegisterIOPortMapEntry(
    PlatformState* platform, const IOPortMapEntry* entry) {
  if (IOPortMapLength(&platform->io_port_map) >= kMaxIOPortMapEntries) {
    return false;
  }
  for (uint8_t i = 0; i < IOPortMapLength(&platform->io_port_map); ++i) {
    IOPortMapEntry* existing_entry = IOPortMapGet(&platform->io_port_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return IOPortMapAppend(&platform->io_port_map, entry);
}

// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
IOPortMapEntry* GetIOPortMapEntryForPort(
    PlatformState* platform, uint16_t port) {
  for (uint8_t i = 0; i < IOPortMapLength(&platform->io_port_map); ++i) {
    IOPortMapEntry* entry = IOPortMapGet(&platform->io_port_map, i);
    if (port >= entry->start && port <= entry->end) {
      return entry;
    }
  }
  return NULL;
}
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
IOPortMapEntry* GetIOPortMapEntryByType(
    PlatformState* platform, IOPortMapEntryType entry_type) {
  for (uint8_t i = 0; i < IOPortMapLength(&platform->io_port_map); ++i) {
    IOPortMapEntry* entry = IOPortMapGet(&platform->io_port_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadIOPortByte(PlatformState* platform, uint16_t port) {
  IOPortMapEntry* entry = GetIOPortMapEntryForPort(platform, port);
  if (!entry || !entry->read_byte) {
    return 0xFF;
  }
  return entry->read_byte(platform, port - entry->start);
}

// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadIOPortWord(PlatformState* platform, uint16_t port) {
  uint8_t low_byte = ReadIOPortByte(platform, port);
  uint8_t high_byte = ReadIOPortByte(platform, port + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WriteIOPortByte(PlatformState* platform, uint16_t port, uint8_t value) {
  IOPortMapEntry* entry = GetIOPortMapEntryForPort(platform, port);
  if (!entry || !entry->write_byte) {
    return;
  }
  entry->write_byte(platform, port - entry->start, value);
}

// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WriteIOPortWord(PlatformState* platform, uint16_t port, uint16_t value) {
  WriteIOPortByte(platform, port, value & 0xFF);
  WriteIOPortByte(platform, port + 1, (value >> 8) & 0xFF);
}
