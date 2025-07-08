#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "interrupts.h"
#include "public.h"
#include "video.h"
#endif  // YAX86_IMPLEMENTATION

void InitBIOS(BIOSState* bios, BIOSConfig* config) {
  // Zero out the BIOS state.
  BIOSState empty_bios = {0};
  *bios = empty_bios;

  bios->config = config;

  MemoryRegionsInit(&bios->memory_regions);
  MemoryRegion conventional_memory = {
      .region_type = kMemoryRegionConventional,
      .start = 0x0000,
      .size = config->memory_size_kb * (2 << 10),
      .read_memory_byte = config->read_memory_byte,
      .write_memory_byte = config->write_memory_byte,
  };
  MemoryRegionsAppend(&bios->memory_regions, &conventional_memory);

  InitVideo(bios);

  // TODO: Set BDA values.
}

// Table of BIOS interrupt handlers, indexed by interrupt number.
YAX86_PRIVATE BIOSInterruptHandler bios_interrupt_handlers[] = {
    0,                                 // 0x00
    0,                                 // 0x01
    0,                                 // 0x02
    0,                                 // 0x03
    0,                                 // 0x04
    HandleBIOSInterrupt05PrintScreen,  // 0x05 - Print screen
    0,                                 // 0x06
    0,                                 // 0x07
    0,                                 // 0x08
    0,                                 // 0x09
    0,                                 // 0x0A
    0,                                 // 0x0B
    0,                                 // 0x0C
    0,                                 // 0x0D
    0,                                 // 0x0E
    0,                                 // 0x0F
    0,                                 // 0x10 - Video I/O
    0,                                 // 0x11 - Equipment determination
    0,                                 // 0x12 - Memory size determination
    0,                                 // 0x13 - Disk I/O
    0,                                 // 0x14 - RS-232 Serial I/O
    0,                                 // 0x15 - Cassette Tape I/O
    0,                                 // 0x16 - Keyboard I/O
    0,                                 // 0x17 - Printer I/O
    0,                                 // 0x18 - ROM BASIC
    0,                                 // 0x19 - Bootstrap Loader
    0,                                 // 0x1A - Time-of-Day

    // HandleBIOSInterrupt10VideoIO,
    // HandleBIOSInterrupt11EquipmentDetermination,
    // HandleBIOSInterrupt12MemorySizeDetermination,
    // HandleBIOSInterrupt13DiskIO,
    // HandleBIOSInterrupt14SerialIO,
    // HandleBIOSInterrupt15CassetteTapeIO,
    // HandleBIOSInterrupt16KeyboardIO,
    // HandleBIOSInterrupt17PrinterIO,
    // HandleBIOSInterrupt18ROMBASIC,
    // HandleBIOSInterrupt19BootstrapLoader,
    // HandleBIOSInterrupt1ATimeOfDay,
};

enum {
  // Number of BIOS interrupt handlers.
  kNumBIOSInterruptHandlers =
      sizeof(bios_interrupt_handlers) / sizeof(bios_interrupt_handlers[0]),
};

// Handle a BIOS interrupt. Follows the handle_interrupt callback signature in
// the CPUConfig structure.
//   - Return kExecuteSuccess if the interrupt was handled and execution
//     should continue.
//   - Return kExecuteUnhandledInterrupt if the interrupt was not handled and
//     should be handled by the VM instead.
//   - Return any other value to terminate the execution loop.
ExecuteStatus HandleBIOSInterrupt(
    BIOSState* bios, CPUState* cpu, uint8_t interrupt_number) {
  uint8_t ah = (cpu->registers[kAX] >> 8) & 0xFF;
  BIOSInterruptHandler handler;
  if (interrupt_number >= kNumBIOSInterruptHandlers ||
      !(handler = bios_interrupt_handlers[interrupt_number])) {
    return kExecuteUnhandledInterrupt;
  }
  return handler(bios, cpu, ah);
}
