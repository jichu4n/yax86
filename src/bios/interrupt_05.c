#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// BIOS interrupt 0x05 - Print screen
// The result of the print screen operation is reported in the status byte at
// address 0x500:
//   - 0x00: Print screen successful
//   - 0x01: Print screen in progress
//   - 0xFF: Print screen failed
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt05PrintScreen(
    BIOSState* bios, __attribute__((unused)) CPUState* cpu,
    __attribute__((unused)) uint8_t ah) {
  WriteMemoryByte(bios, 0x0500, 0x00);
  return kExecuteSuccess;
}
