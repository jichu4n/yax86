#include "bios.h"
#include "pic.h"
#include "ppi.h"

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_PLATFORM_LOG(level, ...) \
  YAX86_LOG(&platform->logger, &kLogModulePlatform, level, __VA_ARGS__)

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

// Record the details of a stop for PlatformGetStopInfo().
static void PlatformRecordStop(
    PlatformState* platform, PlatformStopReason reason, uint8_t index,
    uint32_t address, bool is_write) {
  PlatformStopInfo* stop_info = &platform->stop_info;
  stop_info->reason = reason;
  stop_info->cs = platform->cpu.registers[kCS];
  stop_info->ip = platform->cpu.registers[kIP];
  stop_info->index = index;
  stop_info->address = address;
  stop_info->is_write = is_write;
  platform->has_stop_info = true;
}

// Stop if the access at address falls within an enabled memory watchpoint.
// Only called when has_enabled_memory_watchpoints is set.
static void PlatformCheckMemoryWatchpoints(
    PlatformState* platform, uint32_t address, bool is_write) {
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    const PlatformMemoryWatchpoint* watchpoint =
        &platform->memory_watchpoints[i];
    if (!watchpoint->enabled || address < watchpoint->start ||
        address > watchpoint->end ||
        !(is_write ? watchpoint->on_write : watchpoint->on_read)) {
      continue;
    }
    PlatformRecordStop(
        platform, kPlatformStopMemoryWatchpoint, i, address, is_write);
    // Unlike a breakpoint or a step, a watchpoint fires in the middle of a
    // tick, so the stop is deferred to the end of that tick.
    platform->stop_pending = true;
    // Ask the CPU to hand control back as soon as the instruction in progress
    // finishes. A watchpoint can also fire from a DMA transfer, in which case
    // there is no CPU tick in progress and this is a no-op - PlatformTick()
    // picks the stop up from stop_pending either way.
    CPURequestStop(&platform->cpu);
    return;
  }
}

// Read a byte from a logical memory address.
uint8_t ReadMemoryByte(PlatformState* platform, uint32_t address) {
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, false);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->read_byte) {
    // Logged at debug rather than warning level: scanning unmapped memory is
    // normal on a PC/XT. GLaBIOS reads every byte of 0xF6000-0xF7FFF looking
    // for option ROMs, for instance.
    YAX86_PLATFORM_LOG(
        kLogLevelDebug, "read from unmapped address %05X", address);
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
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, true);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->write_byte) {
    YAX86_PLATFORM_LOG(
        kLogLevelDebug, "write of %02X to unmapped address %05X", value,
        address);
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
    // Unlike unmapped memory, an unmapped port usually means a device is
    // missing from the port map, so this stays at warning level.
    YAX86_PLATFORM_LOG(kLogLevelWarn, "read from unmapped port %04X", port);
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
    YAX86_PLATFORM_LOG(
        kLogLevelWarn, "write of %02X to unmapped port %04X", value, port);
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
// Callbacks for Video module
// ============================================================================

static uint8_t VideoCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return VideoReadPort((VideoState*)entry->context, port);
}

static void VideoCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  VideoWritePort((VideoState*)entry->context, port, value);
}

static uint8_t VideoCallbackReadVRAMByte(
    MemoryMapEntry* entry, uint32_t address) {
  return VideoReadVRAM((VideoState*)entry->context, address);
}

static void VideoCallbackWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  VideoWriteVRAM((VideoState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for BIOS module
// ============================================================================

static uint8_t BIOSCallbackReadROMByte(
    YAX86_UNUSED MemoryMapEntry* entry, uint32_t address) {
  return BIOSReadROMByte(address);
}

// ============================================================================
// Initialization
// ============================================================================

static void PlatformInitBIOS(PlatformState* platform) {
  uint32_t bios_size = BIOSGetROMSize();
  MemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = kMemoryMapEntryBIOSROM,
      .start = kBIOSROMStartAddress,
      .end = kBIOSROMStartAddress + bios_size - 1,
      .read_byte = BIOSCallbackReadROMByte,
      .write_byte = NULL,  // BIOS ROM is read-only.
  };
  RegisterMemoryMapEntry(platform, &bios_rom);
}

// Runs the CPU's interrupt acknowledge cycle against the PIC. This is the
// only path by which an external interrupt reaches the CPU, and the PIC marks
// the interrupt in service as part of it - so a vector is never produced
// unless the CPU is taking it right now.
static bool CPUCallbackAcknowledgeInterrupt(CPUState* cpu, uint8_t* vector) {
  PlatformState* platform = (PlatformState*)cpu->config->context;
  const uint8_t interrupt_vector = PICGetPendingInterrupt(&platform->pic);
  if (interrupt_vector == kPICNoPendingInterrupt) {
    return false;
  } else {
    *vector = interrupt_vector;
    return true;
  }
}

static void PlatformInitCPU(PlatformState* platform) {
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.logger = &platform->logger;
  platform->cpu_config.read_memory_byte = CPUCallbackReadMemoryByte;
  platform->cpu_config.write_memory_byte = CPUCallbackWriteMemoryByte;
  platform->cpu_config.acknowledge_interrupt = CPUCallbackAcknowledgeInterrupt;
  platform->cpu_config.read_port = CPUCallbackReadPortByte;
  platform->cpu_config.write_port = CPUCallbackWritePortByte;
  CPUInit(&platform->cpu, &platform->cpu_config);

  // Initialize CPU registers.
  // CS:IP points to the BIOS entry point at 0xFFFF0.
  platform->cpu.registers[kCS] = 0xF000;
  platform->cpu.registers[kIP] = 0xFFF0;
  platform->cpu.registers[kDS] = 0x0000;
  platform->cpu.registers[kSS] = 0x0000;
  platform->cpu.registers[kES] = 0x0000;
  platform->cpu.registers[kSP] = 0xFFFE;
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

static void PlatformInitPIC(PlatformState* platform) {
  platform->pic_config.sp = false;
  platform->pic_config.logger = &platform->logger;
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
  platform->pit_config.logger = &platform->logger;
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
  platform->ppi_config.logger = &platform->logger;
  platform->ppi_config.num_floppy_drives = 1;
  platform->ppi_config.memory_size = kPPIMemorySize256KB;
  // The DIP switches are what the BIOS branches on to decide which adapter to
  // program, so they have to agree with the adapter the platform registers.
  platform->ppi_config.display_mode =
      platform->config->video_adapter == kVideoAdapterCGA ? kPPIDisplayCGA80x25
                                                          : kPPIDisplayMDA;
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
  platform->keyboard_config.logger = &platform->logger;
  platform->keyboard_config.raise_irq1 = KeyboardCallbackPlatformRaiseIRQ1;
  platform->keyboard_config.send_scancode = KeyboardCallbackSendScancode;
  KeyboardInit(&platform->keyboard, &platform->keyboard_config);
}

static void PlatformInitFDC(PlatformState* platform) {
  platform->fdc_config.context = platform;
  platform->fdc_config.logger = &platform->logger;
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
  platform->dma_config.logger = &platform->logger;
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

static void PlatformInitVideo(PlatformState* platform) {
  platform->video_config = kDefaultVideoConfig;
  platform->video_config.context = platform;
  platform->video_config.logger = &platform->logger;
  platform->video_config.adapter = platform->config->video_adapter;
  VideoInit(&platform->video, &platform->video_config);

  const VideoAdapterMetadata* adapter =
      VideoGetAdapterMetadata(&platform->video);

  MemoryMapEntry vram_entry = {
      .context = &platform->video,
      .entry_type = kMemoryMapEntryVRAM,
      .start = adapter->vram_address,
      .end = adapter->vram_address + adapter->vram_size - 1,
      .read_byte = VideoCallbackReadVRAMByte,
      .write_byte = VideoCallbackWriteVRAMByte,
  };
  RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = &platform->video,
      .entry_type = kPortMapEntryVideo,
      .start = adapter->port_start,
      .end = adapter->port_end,
      .read_byte = VideoCallbackReadPortByte,
      .write_byte = VideoCallbackWritePortByte,
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
  LoggerInit(&platform->logger, config->logger_config);

  PlatformInitCPU(platform);
  PlatformInitMemoryMap(platform);
  PlatformInitBIOS(platform);
  PlatformInitPIC(platform);
  PlatformInitPIT(platform);
  PlatformInitPPI(platform);
  PlatformInitKeyboard(platform);
  PlatformInitFDC(platform);
  PlatformInitDMA(platform);
  PlatformInitVideo(platform);

  platform->ticks = 0;

  PlatformClearBreakpoints(platform);
  PlatformClearMemoryWatchpoints(platform);
  platform->is_step_mode = false;
  platform->has_stop_info = false;
  platform->stop_pending = false;
  platform->skip_breakpoint_check = false;

  return true;
}

bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  PICRaiseIRQ(&platform->pic, irq);
  return true;
}

// Stop if there is an enabled breakpoint on the instruction about to execute.
// Only called when has_enabled_breakpoints is set. Returns true if execution
// should stop.
static bool PlatformCheckBreakpoints(PlatformState* platform) {
  const uint16_t cs = platform->cpu.registers[kCS];
  const uint16_t ip = platform->cpu.registers[kIP];
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    const PlatformBreakpoint* breakpoint = &platform->breakpoints[i];
    if (breakpoint->enabled && breakpoint->cs == cs && breakpoint->ip == ip) {
      PlatformRecordStop(platform, kPlatformStopBreakpoint, i, 0, false);
      return true;
    }
  }
  return false;
}

PlatformRunStatus PlatformTick(PlatformState* platform) {
  // Stop before executing the instruction at a breakpoint. Nothing else in the
  // machine is ticked, because no time has passed yet.
  if (platform->has_enabled_breakpoints && !platform->cpu.is_halted) {
    if (platform->skip_breakpoint_check) {
      // Resuming from a breakpoint stop - execute this instruction rather than
      // stopping on it again.
      platform->skip_breakpoint_check = false;
    } else if (PlatformCheckBreakpoints(platform)) {
      platform->skip_breakpoint_check = true;
      return kPlatformStopped;
    }
  }

  // Tick the CPU.
  CPUTickResult cpu_result = CPUTick(&platform->cpu);

  // The instruction took as long as it took, and every device is clocked from
  // that. Each keeps its own remainder, so a device whose period does not
  // divide the instruction's length still runs at its own rate on average
  // rather than drifting.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;

  platform->pit_cycles += cycles;
  while (platform->pit_cycles >= kCyclesPerPITTick) {
    platform->pit_cycles -= kCyclesPerPITTick;
    PITTick(&platform->pit);
  }

  platform->fdc_cycles += cycles;
  while (platform->fdc_cycles >= kCyclesPerFDCTick) {
    platform->fdc_cycles -= kCyclesPerFDCTick;
    FDCTick(&platform->fdc);
  }

  platform->keyboard_cycles += cycles;
  while (platform->keyboard_cycles >= kCyclesPerMillisecond) {
    platform->keyboard_cycles -= kCyclesPerMillisecond;
    KeyboardTickMs(&platform->keyboard);
  }

  // The video adapter keeps its own cycle remainder, because the MDA and the
  // CGA scan at different rates.
  VideoTick(&platform->video, cycles);

  // A watchpoint may have fired from the CPU or from a DMA transfer.
  if (platform->stop_pending) {
    platform->stop_pending = false;
    return kPlatformStopped;
  }

  if (cpu_result == kCPUTickInvalid) {
    return kPlatformInvalid;
  }

  // A halted CPU with interrupts disabled and nothing pending can never be
  // woken. Note that an ordinary halt is not reported as a stop: the PIT and
  // the rest of the machine must keep ticking so that an interrupt can wake
  // the CPU back up.
  if (platform->cpu.is_halted && !CPUGetFlag(&platform->cpu, kIF) &&
      !platform->cpu.has_pending_internal_interrupt) {
    return kPlatformHung;
  }

  if (platform->is_step_mode && cpu_result == kCPUTickExecuted) {
    PlatformRecordStop(platform, kPlatformStopStep, 0, 0, false);
    return kPlatformStopped;
  }

  return kPlatformRunning;
}

PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  while (platform->ticks - start < max_cycles) {
    PlatformRunStatus status = PlatformTick(platform);
    if (status != kPlatformRunning) {
      return status;
    }
  }
  return kPlatformRunning;
}

// ============================================================================
// Breakpoints and watchpoints
// ============================================================================

// Recompute the cached hot path early-out flags.
static void PlatformUpdateEnabledFlags(PlatformState* platform) {
  platform->has_enabled_breakpoints = false;
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    if (platform->breakpoints[i].enabled) {
      platform->has_enabled_breakpoints = true;
      break;
    }
  }
  platform->has_enabled_memory_watchpoints = false;
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    if (platform->memory_watchpoints[i].enabled) {
      platform->has_enabled_memory_watchpoints = true;
      break;
    }
  }
}

int8_t PlatformAddBreakpoint(
    PlatformState* platform, uint16_t cs, uint16_t ip) {
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    PlatformBreakpoint* breakpoint = &platform->breakpoints[i];
    if (breakpoint->enabled) {
      continue;
    }
    breakpoint->enabled = true;
    breakpoint->cs = cs;
    breakpoint->ip = ip;
    platform->has_enabled_breakpoints = true;
    return (int8_t)i;
  }
  return kInvalidWatchIndex;
}

bool PlatformRemoveBreakpoint(PlatformState* platform, uint8_t index) {
  if (index >= kMaxBreakpoints || !platform->breakpoints[index].enabled) {
    return false;
  }
  platform->breakpoints[index].enabled = false;
  PlatformUpdateEnabledFlags(platform);
  return true;
}

void PlatformClearBreakpoints(PlatformState* platform) {
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    platform->breakpoints[i].enabled = false;
  }
  platform->has_enabled_breakpoints = false;
}

int8_t PlatformAddMemoryWatchpoint(
    PlatformState* platform, uint32_t start, uint32_t end, bool on_read,
    bool on_write) {
  if (start > end || (!on_read && !on_write)) {
    return kInvalidWatchIndex;
  }
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    PlatformMemoryWatchpoint* watchpoint = &platform->memory_watchpoints[i];
    if (watchpoint->enabled) {
      continue;
    }
    watchpoint->enabled = true;
    watchpoint->start = start;
    watchpoint->end = end;
    watchpoint->on_read = on_read;
    watchpoint->on_write = on_write;
    platform->has_enabled_memory_watchpoints = true;
    return (int8_t)i;
  }
  return kInvalidWatchIndex;
}

bool PlatformRemoveMemoryWatchpoint(PlatformState* platform, uint8_t index) {
  if (index >= kMaxMemoryWatchpoints ||
      !platform->memory_watchpoints[index].enabled) {
    return false;
  }
  platform->memory_watchpoints[index].enabled = false;
  PlatformUpdateEnabledFlags(platform);
  return true;
}

void PlatformClearMemoryWatchpoints(PlatformState* platform) {
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    platform->memory_watchpoints[i].enabled = false;
  }
  platform->has_enabled_memory_watchpoints = false;
}

void PlatformSetStepMode(PlatformState* platform, bool is_step_mode) {
  platform->is_step_mode = is_step_mode;
}

const PlatformStopInfo* PlatformGetStopInfo(const PlatformState* platform) {
  return platform->has_stop_info ? &platform->stop_info : NULL;
}
