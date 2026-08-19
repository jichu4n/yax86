#include "bios.h"
#include "pic.h"
#include "ppi.h"

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_PLATFORM_LOG(level, ...) \
  YAX86_LOG(&platform->logger, &kLogModulePlatform, level, __VA_ARGS__)

// How long until the next device needs attention, in CPU cycles from now.
static uint32_t PlatformCyclesUntilNextEvent(
    const PlatformState* platform, uint32_t max_cycles);

enum {
  // Never let a deadline sit further out than this, so that a machine in which
  // nothing is scheduled still comes back regularly.
  //
  // Note that this is not a bound on how many cycles accumulate between syncs.
  // A deadline is only tested between instructions, and a REP string
  // instruction retires as a single tick charging thousands of cycles, so a
  // sync interval routinely runs past it - measured at 27,844 cycles, or
  // 5.8ms, over a DOS boot. Anything relying on a bound has to impose its own;
  // see kMaxIdleSkipCycles.
  kMaxEventInterval = kCyclesPerMillisecond,

  // The furthest an idle skip may move the clock in one go.
  //
  // A skip is otherwise bounded only by the budget its caller passed to
  // PlatformRun(), which nothing checks. Every device's catch-up arithmetic in
  // PlatformSync() is 32-bit and accumulates into a counter holding a
  // remainder, so a jump approaching 2^32 would overflow it, and the keyboard
  // is caught up a millisecond per iteration, so it would also spend ~900,000
  // of them in a single call. A tenth of a second is far more than any sane
  // budget and far less than either limit.
  kMaxIdleSkipCycles = kCPUCyclesPerSecond / 10,
};

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
  if (entry) {
    // Plain storage, which is what every region except video memory is. Going
    // straight to the buffer saves an indirect call on the hottest path in the
    // emulator: every byte of every instruction is fetched through here.
    if (entry->read_data) {
      return entry->read_data[address - entry->start];
    }
    if (entry->read_byte_fn) {
      return entry->read_byte_fn(entry, address - entry->start);
    }
  }
  // Logged at debug rather than warning level: scanning unmapped memory is
  // normal on a PC/XT. GLaBIOS reads every byte of 0xF6000-0xF7FFF looking
  // for option ROMs, for instance.
  YAX86_PLATFORM_LOG(
      kLogLevelDebug, "read from unmapped address %05X", address);
  return 0xFF;
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
  if (entry) {
    if (entry->write_data) {
      entry->write_data[address - entry->start] = value;
      return;
    }
    if (entry->write_byte_fn) {
      entry->write_byte_fn(entry, address - entry->start, value);
      return;
    }
  }
  // Either unmapped, or a read-only region such as a ROM. Both discard the
  // write, and both are logged, as they were before there was a direct path.
  YAX86_PLATFORM_LOG(
      kLogLevelDebug, "write of %02X to unmapped address %05X", value, address);
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
  // A guest timing loop reads the counter expecting it to have moved, so the
  // PIT has to be caught up before it is read.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return PITReadPort(&platform->pit, port);
}

static void PITCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // Syncing first applies the cycles that ran under the old configuration;
  // syncing again afterwards reschedules against the new one.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  PITWritePort(&platform->pit, port, value);
  PlatformSync(platform);
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
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return PPIReadPort(&platform->ppi, port);
}

static void PPICallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // Port B gates the speaker against PIT channel 2, so what the guest hears
  // depends on the channel's output state being current.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  PPIWritePort(&platform->ppi, port, value);
  PlatformSync(platform);
}

static void PPICallbackSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock_low) {
  PlatformState* platform = (PlatformState*)context;
  KeyboardHandleControl(
      &platform->keyboard, keyboard_enable_clear, keyboard_clock_low);
}

static void PPICallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  PlatformState* platform = (PlatformState*)context;
  if (platform->config->set_pc_speaker_frequency) {
    platform->config->set_pc_speaker_frequency(platform, frequency_hz);
  }
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
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return FDCReadPort(&platform->fdc, port);
}

static void FDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // A write can start a command, which is what puts the controller into the
  // execution phase the scheduler watches for.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  FDCWritePort(&platform->fdc, port, value);
  PlatformSync(platform);
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
  // The status port reports where the CRT beam is, which is only meaningful
  // once the beam has been advanced to now. Guests poll this to wait for
  // retrace.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return VideoReadPort(&platform->video, port);
}

static void VideoCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  VideoWritePort(&platform->video, port, value);
}

static void VideoCallbackWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  VideoWriteVRAM((VideoState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for HDC module
// ============================================================================

static uint8_t HDCCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return HDCReadPort((HDCState*)entry->context, port);
}

static void HDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  HDCWritePort((HDCState*)entry->context, port, value);
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
      // The ROM image is a constant array in the library, so it is read
      // directly. write_data is left NULL because the BIOS ROM is read-only.
      .read_data = BIOSGetROMData(),
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

enum {
  // The DOS idle interrupt. MS-DOS issues it from the loops in which it waits
  // for input, to tell anything listening that the machine has nothing to do.
  kDOSIdleInterrupt = 0x28,
};

// Notes the guest declaring itself idle. Nothing is serviced here - the guest's
// own handler runs as usual - so this only ever reports the interrupt onwards.
//
// Only installed when the idle skip is enabled, so a machine without it pays
// nothing per interrupt.
static InterruptHandlerResult CPUCallbackHandleInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  if (interrupt_number == kDOSIdleInterrupt) {
    PlatformState* platform = (PlatformState*)cpu->config->context;
    platform->is_guest_idle = true;
  }
  return kInterruptHandlerUnhandled;
}

static void PlatformInitCPU(PlatformState* platform) {
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.logger = &platform->logger;
  platform->cpu_config.read_memory_byte = CPUCallbackReadMemoryByte;
  platform->cpu_config.write_memory_byte = CPUCallbackWriteMemoryByte;
  platform->cpu_config.acknowledge_interrupt = CPUCallbackAcknowledgeInterrupt;
  if (platform->config->enable_dos_idle_skip) {
    platform->cpu_config.handle_interrupt = CPUCallbackHandleInterrupt;
  }
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
      // Conventional memory is the caller's buffer, accessed directly.
      .read_data = platform->config->physical_memory,
      .write_data = platform->config->physical_memory};
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
      .context = platform,
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
  platform->ppi_config.set_pc_speaker_frequency =
      PPICallbackSetPCSpeakerFrequency;
  platform->ppi_config.set_keyboard_control = PPICallbackSetKeyboardControl;
  PPIInit(&platform->ppi, &platform->ppi_config);
  PortMapEntry ppi_entry = {
      .entry_type = kPortMapEntryPPI,
      .start = 0x60,
      .end = 0x63,
      .read_byte = PPICallbackReadPortByte,
      .write_byte = PPICallbackWritePortByte,
      .context = platform,
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
      .context = platform,
  };
  RegisterPortMapEntry(platform, &fdc_entry);
}

static void PlatformInitHDC(PlatformState* platform) {
  platform->hdc_config.context = platform;
  platform->hdc_config.logger = &platform->logger;
  HDCInit(&platform->hdc, &platform->hdc_config);

  const uint32_t option_rom_size = HDCGetOptionROMSize();
  MemoryMapEntry option_rom_entry = {
      .context = &platform->hdc,
      .entry_type = kMemoryMapEntryHDCOptionROM,
      .start = kHDCOptionROMStartAddress,
      .end = kHDCOptionROMStartAddress + option_rom_size - 1,
      // As with the BIOS ROM, a constant array read directly. write_data is
      // left NULL because the option ROM is read-only.
      .read_data = HDCGetOptionROMData(),
  };
  RegisterMemoryMapEntry(platform, &option_rom_entry);

  PortMapEntry port_entry = {
      .context = &platform->hdc,
      .entry_type = (PortMapEntryType)kPortMapEntryHDC,
      .start = kHDCPortBase,
      .end = kHDCPortBase + kHDCNumPorts - 1,
      .read_byte = HDCCallbackReadPortByte,
      .write_byte = HDCCallbackWritePortByte,
  };
  RegisterPortMapEntry(platform, &port_entry);
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
  platform->video_config.vram = platform->config->vram;
  VideoInit(&platform->video, &platform->video_config);

  const VideoAdapterMetadata* adapter =
      VideoGetAdapterMetadata(&platform->video);

  MemoryMapEntry vram_entry = {
      .context = &platform->video,
      .entry_type = kMemoryMapEntryVRAM,
      .start = adapter->vram_address,
      .end = adapter->vram_address + adapter->vram_size - 1,
      // Reads go straight to the buffer: nothing observes them, and the guest
      // reads back what it wrote. Writes keep a callback so the adapter can
      // see them - see VideoWriteVRAMByte.
      .read_data = platform->config->vram,
      .write_byte_fn = VideoCallbackWriteVRAMByte,
  };
  RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = platform,
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
  platform->config = config;
  // Initialized first, ahead of validation, so that a rejected config can
  // still be logged.
  LoggerInit(&platform->logger, config->logger_config);

  if (config->physical_memory_size < kMinPhysicalMemorySize ||
      config->physical_memory_size > kMaxPhysicalMemorySize) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "physical_memory_size %u is not between %u and %u bytes",
        (unsigned)config->physical_memory_size,
        (unsigned)kMinPhysicalMemorySize, (unsigned)kMaxPhysicalMemorySize);
    return false;
  }
  // A machine with no memory would run until its first instruction fetch came
  // back as open bus, so this is rejected here rather than left to fail
  // obscurely later.
  if (config->physical_memory == NULL) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "no physical_memory buffer was provided");
    return false;
  }
  // The video adapter reads and writes this directly, so an absent buffer
  // would leave the screen permanently blank with nothing to say why.
  if (config->vram == NULL) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "no vram buffer was provided");
    return false;
  }

  PlatformInitCPU(platform);
  PlatformInitMemoryMap(platform);
  PlatformInitBIOS(platform);
  PlatformInitPIC(platform);
  PlatformInitPIT(platform);
  PlatformInitPPI(platform);
  PlatformInitKeyboard(platform);
  PlatformInitFDC(platform);
  PlatformInitHDC(platform);
  PlatformInitDMA(platform);
  PlatformInitVideo(platform);

  platform->ticks = 0;
  platform->pit_cycles = 0;
  platform->fdc_cycles = 0;
  platform->keyboard_cycles = 0;
  platform->last_sync_ticks = 0;
  // Schedule the first deadline from the devices' power-on state, so that the
  // first instruction is not treated as already overdue.
  platform->next_event_ticks =
      PlatformCyclesUntilNextEvent(platform, kMaxEventInterval);

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

// ============================================================================
// Device scheduling
// ============================================================================
//
// Devices are not clocked on every instruction. Each is brought up to date
// only when it next has something to do, and the instruction path compares
// against the earliest of those deadlines.
//
// The consequence is that device state is generally stale. Anything that reads
// it - a port handler, or the host asking what the CRT beam is doing - has to
// bring the device up to date first, which is what PlatformSync() is
// for. Getting this wrong shows up as a guest timing loop reading a counter
// that never moves, so port handlers that touch a device call it.

// Whether the earliest device deadline has come due. Compared as a signed
// difference so that a deadline stays in the future across the point where the
// 32-bit cycle counter wraps.
static inline bool PlatformIsEventDue(const PlatformState* platform) {
  return (int32_t)(platform->ticks - platform->next_event_ticks) >= 0;
}

// Work out when the next device needs attention, in cycles from now, up to
// max_cycles. The instruction path passes kMaxEventInterval; the idle skip
// passes what is left of the budget it was given, because there the answer is
// how far the clock may be moved rather than how long until it is checked
// again.
static uint32_t PlatformCyclesUntilNextEvent(
    const PlatformState* platform, uint32_t max_cycles) {
  uint32_t cycles = max_cycles;

  // The PIT's next output change, converted from its own 1.19MHz clock and
  // offset by the cycles already credited towards its next tick.
  const uint32_t pit_ticks = PITTicksUntilNextEvent(&platform->pit);
  if (pit_ticks != kPITNoEvent) {
    const uint32_t pit_cycles =
        pit_ticks * kCyclesPerPITTick - platform->pit_cycles;
    if (pit_cycles < cycles) {
      cycles = pit_cycles;
    }
  }

  // The floppy controller only advances a command that is executing; when no
  // command is in flight there is nothing for a tick to do.
  if (platform->fdc.phase == kFDCPhaseExecution) {
    const uint32_t fdc_cycles = kCyclesPerFDCTick - platform->fdc_cycles;
    if (fdc_cycles < cycles) {
      cycles = fdc_cycles;
    }
  }

  // Never return 0, which would leave the deadline permanently due.
  return cycles > 0 ? cycles : 1;
}

// Bring every device up to date with the cycles that have run since the last
// sync, and schedule the next deadline.
void PlatformSync(PlatformState* platform) {
  const uint32_t elapsed = platform->ticks - platform->last_sync_ticks;
  platform->last_sync_ticks = platform->ticks;

  if (elapsed > 0) {
    platform->pit_cycles += elapsed;
    const uint32_t pit_ticks = platform->pit_cycles / kCyclesPerPITTick;
    if (pit_ticks > 0) {
      platform->pit_cycles -= pit_ticks * kCyclesPerPITTick;
      PITAdvance(&platform->pit, pit_ticks);
    }

    platform->fdc_cycles += elapsed;
    const uint32_t fdc_ticks = platform->fdc_cycles / kCyclesPerFDCTick;
    if (fdc_ticks > 0) {
      platform->fdc_cycles -= fdc_ticks * kCyclesPerFDCTick;
      // FDCTick() does nothing unless a command is executing, so an idle
      // controller does not need catching up at all - which matters because
      // an idle stretch is not bounded by an FDC deadline.
      if (platform->fdc.phase == kFDCPhaseExecution) {
        for (uint32_t i = 0; i < fdc_ticks; ++i) {
          FDCTick(&platform->fdc);
        }
      }
    }

    platform->keyboard_cycles += elapsed;
    const uint32_t keyboard_ticks =
        platform->keyboard_cycles / kCyclesPerMillisecond;
    if (keyboard_ticks > 0) {
      platform->keyboard_cycles -= keyboard_ticks * kCyclesPerMillisecond;
      for (uint32_t i = 0; i < keyboard_ticks; ++i) {
        KeyboardTickMs(&platform->keyboard);
      }
    }

    // Advancing the beam costs the same whether it moved by one cycle or a
    // whole frame, so the video adapter needs no deadline of its own.
    VideoTick(&platform->video, elapsed);
  }

  platform->next_event_ticks =
      platform->ticks +
      PlatformCyclesUntilNextEvent(platform, kMaxEventInterval);
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
  // that - but in arrears. Rather than offering each device its share of the
  // cycles on every instruction, this only checks whether the earliest device
  // deadline has come due, which is one comparison in the common case.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;
  if (PlatformIsEventDue(platform)) {
    PlatformSync(platform);
  }

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

// Advance the clock over time the guest has said it has no use for, up to
// max_cycles. Every device is brought up to date across the skipped interval
// rather than having it taken away from them, so the guest's timer tick count
// is the same either way - what is skipped is executing the loop it would have
// spent the interval in.
//
// The bound matters as much as the skip: without it a machine idling at a DOS
// prompt would be handed more emulated time per call than the caller asked for,
// and would run its clock fast. kMaxIdleSkipCycles bounds it a second time, in
// case the caller's budget is itself large enough to overflow the catch-up
// arithmetic.
static void PlatformSkipIdleTime(PlatformState* platform, uint32_t max_cycles) {
  if (max_cycles > kMaxIdleSkipCycles) {
    max_cycles = kMaxIdleSkipCycles;
  }
  const uint32_t cycles = PlatformCyclesUntilNextEvent(platform, max_cycles);
  if (cycles == 0) {
    return;
  }
  platform->ticks += cycles;
  PlatformSync(platform);
}

PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  uint32_t elapsed = 0;
  while (elapsed < max_cycles) {
    PlatformRunStatus status = PlatformTick(platform);
    if (status != kPlatformRunning) {
      return status;
    }
    elapsed = platform->ticks - start;
    // Skipping is done here rather than where the guest declared itself idle
    // because only this loop knows how much of the budget is left, which is
    // what bounds how far the clock may move.
    if (platform->is_guest_idle) {
      platform->is_guest_idle = false;
      if (elapsed < max_cycles) {
        PlatformSkipIdleTime(platform, max_cycles - elapsed);
        elapsed = platform->ticks - start;
      }
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
