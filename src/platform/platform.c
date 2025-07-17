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

static uint8_t ReadBIOSROMByte(MemoryMapEntry* entry, uint32_t address) {
  PlatformState* platform = (PlatformState*)entry->context;
  if (platform->config && platform->config->read_bios_rom_byte) {
    return platform->config->read_bios_rom_byte(platform, address);
  }
  return 0xFF;
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
//   - The BIOS ROM size is not between 0 and 64K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config) {
  if (config->physical_memory_size < kMinPhysicalMemorySize ||
      config->physical_memory_size > kMaxPhysicalMemorySize) {
    return false;
  }
  if (config->bios_rom_size > kMaxBIOSROMSize) {
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
  if (config->bios_rom_size > 0) {
    MemoryMapEntry bios_rom = {
        .context = platform,
        .entry_type = kMemoryMapEntryBIOSROM,
        .start = 0xF0000,
        .end = 0xF0000 + config->bios_rom_size - 1,
        .read_byte = ReadBIOSROMByte,
        .write_byte = NULL,  // BIOS ROM is read-only.
    };
    MemoryMapAppend(&platform->memory_map, &bios_rom);
  }

  return true;
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
