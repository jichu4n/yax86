// Guest RAM above what SRAM holds, kept in PSRAM and paged through frames in
// SRAM: the host's half of the core's YPlatformPagedMemory.
//
// A page is brought into a frame when the guest first touches it, from PSRAM,
// or as zeros if it has never been written back. A frame is taken back by
// the clock algorithm: a hand goes round the frames, clearing the
// RESIDENT flag of each page it passes that has been used since, and takes
// the first that has not - written back to PSRAM first if it was written. A
// cleared page used again costs only a call that sets the flag again.
//
// Called from core 0 only.
#ifndef YAX86_PICOCALC_PAGER_H
#define YAX86_PICOCALC_PAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/platform.h"

enum {
  // A page, and a frame: 256 bytes, the smallest whose page table - 5KB for
  // 640KB - the SRAM has room for. On the device, a Word 5.5 session spent
  // 1.3% of its time in faults with 256-byte pages, 2.8% with 512 and 6.9%
  // with 1KB, and smaller pages cost nothing measurable where nothing faults.
  kPagerPageShift = 8,
  kPagerPageSize = 1 << kPagerPageShift,
};

typedef struct PagerStats {
  uint32_t frames;
  // Pages brought into a frame from PSRAM, and as zeros.
  uint32_t page_ins;
  uint32_t zero_fills;
  // Pages written back to PSRAM on leaving their frame.
  uint32_t write_backs;
  // Calls for a page already in its frame, which only set a flag.
  uint32_t soft_faults;
  // PSRAM transfers that failed, leaving the page's bytes wrong.
  uint32_t errors;
  // Time spent in the page fault handler.
  uint64_t fault_us;
} PagerStats;

// Page size bytes of guest RAM through the frames, which take frames_size
// bytes. Returns the memory to hand the platform as
// YPlatformConfig.paged_memory.
YPlatformPagedMemory* PagerInit(
    uint8_t* frames, uint32_t frames_size, uint32_t size);

PagerStats PagerGetStats(void);

#endif  // YAX86_PICOCALC_PAGER_H
