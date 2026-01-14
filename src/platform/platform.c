#include "pic.h"
#include "ppi.h"

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#else
// When bundled, bios.h is already included before platform.h in all.c
#endif  // YAX86_IMPLEMENTATION

#ifndef YAX86_IMPLEMENTATION
// Include BIOS ROM data for BIOS memory mapping.
#include "../bios/bios_rom_data.h"
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

// ============================================================================
// Callbacks for CPU module
// ============================================================================

static uint8_t CPUCallbackReadMemoryByte(CPUState* cpu, uint32_t address) {
  return ReadMemoryByte((PlatformState*)cpu->config->context, address);
}

static void CPUCallbackWriteMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  WriteMemoryByte((PlatformState*)cpu->config->context, address, value);
}

static uint8_t CPUCallbackReadPortByte(CPUState* cpu, uint16_t port) {
  return ReadPortByte((PlatformState*)cpu->config->context, port);
}

static void CPUCallbackWritePortByte(
    CPUState* cpu, uint16_t port, uint8_t value) {
  WritePortByte((PlatformState*)cpu->config->context, port, value);
}

// Callback for the CPU to check for pending interrupts from the PIC after an
// instruction has been executed. This is how we connect the PIC to the CPU's
// interrupt handling flow.
static ExecuteStatus CPUCallbackOnAfterExecuteInstruction(
    CPUState* cpu, YAX86_UNUSED const struct Instruction* instruction) {
  PlatformState* platform = (PlatformState*)cpu->config->context;

  FDCTick(&platform->fdc);

  if (!GetFlag(cpu, kIF)) {
    return kExecuteSuccess;
  }

  uint8_t interrupt_vector = PICGetPendingInterrupt(&platform->pic);
  if (interrupt_vector != kPICNoPendingInterrupt) {
    SetPendingInterrupt(cpu, interrupt_vector);
  }

  return kExecuteSuccess;
}

static const CPUConfig kEmptyCPUConfig = {0};

// ============================================================================
// Callbacks for physical memory
// ============================================================================
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

// ============================================================================
// Callbacks for 8259 PIC module
// ============================================================================

static uint8_t PICCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PICReadPort((PICState*)entry->context, port);
}

static void PICCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PICWritePort((PICState*)entry->context, port, value);
}

static void PICCallbackPlatformRaiseIRQ0(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 0);
}

// ============================================================================
// Callbacks for 8253 PIT module
// ============================================================================

static uint8_t PITCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PITReadPort((PITState*)entry->context, port);
}

static void PITCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PITWritePort((PITState*)entry->context, port, value);
}

static void PITCallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  PlatformState* platform = (PlatformState*)context;
  PPISetPCSpeakerFrequencyFromPIT(&platform->ppi, frequency_hz);
}

// ============================================================================
// Callbacks for 8255 PPI module
// ============================================================================

static uint8_t PPICallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PPIReadPort((PPIState*)entry->context, port);
}

static void PPICallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PPIWritePort((PPIState*)entry->context, port, value);
}

static void PPICallbackSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock_low) {
  PlatformState* platform = (PlatformState*)context;
  KeyboardHandleControl(
      &platform->keyboard, keyboard_enable_clear, keyboard_clock_low);
}

// ============================================================================
// Callbacks for Keyboard module
// ============================================================================

static void KeyboardCallbackPlatformRaiseIRQ1(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 1);
}

static void KeyboardCallbackSendScancode(void* context, uint8_t scancode) {
  PlatformState* platform = (PlatformState*)context;
  PPISetScancode(&platform->ppi, scancode);
}

// ============================================================================
// Callbacks for uPD765 FDC module
// ============================================================================

enum {
  kPlatformDMAChannelFloppy = 2,
};

static void FDCCallbackRaiseIRQ6(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 6);
}

static void FDCCallbackRequestDMA(void* context) {
  PlatformState* platform = (PlatformState*)context;
  DMATransferByte(&platform->dma, kPlatformDMAChannelFloppy);
}

static uint8_t FDCCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return FDCReadPort((FDCState*)entry->context, port);
}

static void FDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  FDCWritePort((FDCState*)entry->context, port, value);
}

// ============================================================================
// Callbacks for DMA module
// ============================================================================

static uint8_t DMACallbackReadMemoryByte(void* context, uint32_t address) {
  PlatformState* platform = (PlatformState*)context;
  return ReadMemoryByte(platform, address);
}

static void DMACallbackWriteMemoryByte(
    void* context, uint32_t address, uint8_t value) {
  PlatformState* platform = (PlatformState*)context;
  WriteMemoryByte(platform, address, value);
}

static uint8_t DMACallbackReadDeviceByte(void* context, uint8_t channel) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      return FDCReadPort(&platform->fdc, kFDCPortData);
    default:
      return 0xFF;
  }
}

static void DMACallbackWriteDeviceByte(
    void* context, uint8_t channel, uint8_t value) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      FDCWritePort(&platform->fdc, kFDCPortData, value);
      break;
    default:
      break;
  }
}

static void DMACallbackOnTerminalCount(void* context, uint8_t channel) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      FDCHandleTC(&platform->fdc);
      break;
    default:
      break;
  }
}

static uint8_t DMACallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return DMAReadPort((DMAState*)entry->context, port);
}

static void DMACallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  DMAWritePort((DMAState*)entry->context, port, value);
}

// ============================================================================
// Callbacks for MDA module
// ============================================================================

static uint8_t MDACallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return MDAReadPort((MDAState*)entry->context, port);
}

static void MDACallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  MDAWritePort((MDAState*)entry->context, port, value);
}

static uint8_t MDACallbackReadVRAMByte(
    MemoryMapEntry* entry, uint32_t address) {
  return MDAReadVRAM((MDAState*)entry->context, address);
}

static void MDACallbackWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  MDAWriteVRAM((MDAState*)entry->context, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

static void PlatformInitCPU(PlatformState* platform) {
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.read_memory_byte = CPUCallbackReadMemoryByte;
  platform->cpu_config.write_memory_byte = CPUCallbackWriteMemoryByte;
  platform->cpu_config.read_port = CPUCallbackReadPortByte;
  platform->cpu_config.write_port = CPUCallbackWritePortByte;
  platform->cpu_config.on_after_execute_instruction =
      CPUCallbackOnAfterExecuteInstruction;
  CPUInit(&platform->cpu, &platform->cpu_config);
}

static void PlatformInitMemoryMap(PlatformState* platform) {
  MemoryMapInit(&platform->memory_map);
  MemoryMapEntry conventional_memory = {
      .context = platform,
      .entry_type = kMemoryMapEntryConventional,
      .start = 0x0000,
      .end = platform->config->physical_memory_size - 1,
      .read_byte = ReadPhysicalMemoryByte,
      .write_byte = WritePhysicalMemoryByte};
  MemoryMapAppend(&platform->memory_map, &conventional_memory);
}

// ============================================================================
// Callbacks for BIOS ROM
// ============================================================================

static uint8_t BIOSROMCallbackReadByte(
    YAX86_UNUSED MemoryMapEntry* entry, uint32_t address) {
  if (address >= kBIOSROMDataSize) {
    return 0xFF;
  }
  return kBIOSROMData[address];
}

static void PlatformInitBIOS(PlatformState* platform) {
  MemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = kMemoryMapEntryBIOSROM,
      .start = 0xF0000,
      .end = 0xF0000 + kBIOSROMDataSize - 1,
      .read_byte = BIOSROMCallbackReadByte,
      .write_byte = NULL,  // BIOS ROM is read-only.
  };
  RegisterMemoryMapEntry(platform, &bios_rom);
}

static void PlatformInitPIC(PlatformState* platform) {
  platform->pic_config.sp = false;
  PICInit(&platform->pic, &platform->pic_config);
  PortMapEntry pic_entry = {
      .entry_type = kPortMapEntryPIC,
      .start = 0x20,
      .end = 0x21,
      .read_byte = PICCallbackReadPortByte,
      .write_byte = PICCallbackWritePortByte,
      .context = &platform->pic,
  };
  RegisterPortMapEntry(platform, &pic_entry);
}

static void PlatformInitPIT(PlatformState* platform) {
  platform->pit_config.context = platform;
  platform->pit_config.raise_irq_0 = PICCallbackPlatformRaiseIRQ0;
  platform->pit_config.set_pc_speaker_frequency =
      PITCallbackSetPCSpeakerFrequency;
  PITInit(&platform->pit, &platform->pit_config);
  PortMapEntry pit_entry = {
      .entry_type = kPortMapEntryPIT,
      .start = 0x40,
      .end = 0x43,
      .read_byte = PITCallbackReadPortByte,
      .write_byte = PITCallbackWritePortByte,
      .context = &platform->pit,
  };
  RegisterPortMapEntry(platform, &pit_entry);
}

static void PlatformInitPPI(PlatformState* platform) {
  platform->ppi_config.context = platform;
  platform->ppi_config.num_floppy_drives = 1;
  platform->ppi_config.memory_size = kPPIMemorySize256KB;
  platform->ppi_config.display_mode = kPPIDisplayMDA;
  platform->ppi_config.fpu_installed = false;
  platform->ppi_config.set_pc_speaker_frequency = NULL;  // TODO
  platform->ppi_config.set_keyboard_control = PPICallbackSetKeyboardControl;
  PPIInit(&platform->ppi, &platform->ppi_config);
  PortMapEntry ppi_entry = {
      .entry_type = kPortMapEntryPPI,
      .start = 0x60,
      .end = 0x63,
      .read_byte = PPICallbackReadPortByte,
      .write_byte = PPICallbackWritePortByte,
      .context = &platform->ppi,
  };
  RegisterPortMapEntry(platform, &ppi_entry);
}

static void PlatformInitKeyboard(PlatformState* platform) {
  platform->keyboard_config.context = platform;
  platform->keyboard_config.raise_irq1 = KeyboardCallbackPlatformRaiseIRQ1;
  platform->keyboard_config.send_scancode = KeyboardCallbackSendScancode;
  KeyboardInit(&platform->keyboard, &platform->keyboard_config);
}

static void PlatformInitFDC(PlatformState* platform) {
  platform->fdc_config.context = platform;
  platform->fdc_config.raise_irq6 = FDCCallbackRaiseIRQ6;
  platform->fdc_config.request_dma = FDCCallbackRequestDMA;
  platform->fdc_config.read_image_byte = NULL;
  platform->fdc_config.write_image_byte = NULL;
  FDCInit(&platform->fdc, &platform->fdc_config);
  PortMapEntry fdc_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryFDC,
      .start = 0x3F0,
      .end = 0x3F7,
      .read_byte = FDCCallbackReadPortByte,
      .write_byte = FDCCallbackWritePortByte,
      .context = &platform->fdc,
  };
  RegisterPortMapEntry(platform, &fdc_entry);
}

static void PlatformInitDMA(PlatformState* platform) {
  platform->dma_config.context = platform;
  platform->dma_config.read_memory_byte = DMACallbackReadMemoryByte;
  platform->dma_config.write_memory_byte = DMACallbackWriteMemoryByte;
  platform->dma_config.read_device_byte = DMACallbackReadDeviceByte;
  platform->dma_config.write_device_byte = DMACallbackWriteDeviceByte;
  platform->dma_config.on_terminal_count = DMACallbackOnTerminalCount;
  DMAInit(&platform->dma, &platform->dma_config);
  PortMapEntry dma_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryDMA,
      .start = 0x00,
      .end = 0x0F,
      .read_byte = DMACallbackReadPortByte,
      .write_byte = DMACallbackWritePortByte,
      .context = &platform->dma,
  };
  RegisterPortMapEntry(platform, &dma_entry);
  PortMapEntry dma_page_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryDMAPage,
      .start = 0x80,
      .end = 0x8F,
      .read_byte = DMACallbackReadPortByte,
      .write_byte = DMACallbackWritePortByte,
      .context = &platform->dma,
  };
  RegisterPortMapEntry(platform, &dma_page_entry);
}

static void PlatformInitMDA(PlatformState* platform) {
  platform->mda_config = kDefaultMDAConfig;
  platform->mda_config.context = platform;
  MDAInit(&platform->mda, &platform->mda_config);

  MemoryMapEntry vram_entry = {
      .context = &platform->mda,
      .entry_type = kMemoryMapEntryMDAVRAM,
      .start = kMDAModeMetadata.vram_address,
      .end = kMDAModeMetadata.vram_address + kMDAModeMetadata.vram_size - 1,
      .read_byte = MDACallbackReadVRAMByte,
      .write_byte = MDACallbackWriteVRAMByte,
  };
  RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = &platform->mda,
      .entry_type = kPortMapEntryMDA,
      .start = 0x3B0,
      .end = 0x3BF,
      .read_byte = MDACallbackReadPortByte,
      .write_byte = MDACallbackWritePortByte,
  };
  RegisterPortMapEntry(platform, &port_entry);
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

  PlatformInitCPU(platform);
  PlatformInitMemoryMap(platform);
  PlatformInitBIOS(platform);
  PlatformInitPIC(platform);
  PlatformInitPIT(platform);
  PlatformInitPPI(platform);
  PlatformInitKeyboard(platform);
  PlatformInitFDC(platform);
  PlatformInitDMA(platform);
  PlatformInitMDA(platform);

  return true;
}

bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  PICRaiseIRQ(&platform->pic, irq);
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
