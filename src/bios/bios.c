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

static inline uint8_t CPUReadMemoryByte(CPUState* cpu, uint32_t address) {
  return ReadMemoryByte((BIOSState*)cpu->config->context, address);
}

static inline void CPUWriteMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  WriteMemoryByte((BIOSState*)cpu->config->context, address, value);
}

static inline ExecuteStatus CPUHandleBIOSInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  return HandleBIOSInterrupt(
      (BIOSState*)cpu->config->context, cpu, interrupt_number);
}

// Register BIOS handlers on the CPU. This should be invoked after InitBIOS to
// configure the CPU to invoke the BIOS for memory access and interrupt
// handling.
void RegisterBIOSHandlers(BIOSState* bios, CPUState* cpu) {
  cpu->config->context = bios;
  cpu->config->read_memory_byte = CPUReadMemoryByte;
  cpu->config->write_memory_byte = CPUWriteMemoryByte;
  cpu->config->handle_interrupt = CPUHandleBIOSInterrupt;
}
