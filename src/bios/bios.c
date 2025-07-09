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
