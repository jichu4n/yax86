#ifndef YAX86_IMPLEMENTATION
#include "interrupts.h"

#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

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

// Helper function to execute a BIOS interrupt function handler based on the AH
// register value.
extern ExecuteStatus ExecuteBIOSInterruptFunctionHandler(
    BIOSInterruptFunctionHandler* handlers, size_t num_handlers,
    BIOSState* bios, CPUState* cpu, uint8_t ah) {
  // When invoked with invalid AH value, no-op and return.
  if (ah >= num_handlers || !handlers[ah]) {
    return kExecuteSuccess;
  }
  return handlers[ah](bios, cpu);
}
