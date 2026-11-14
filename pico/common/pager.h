// Guest RAM kept in PSRAM and paged through frames in SRAM: the host's half of
// the core's YPlatformPagedMemory.
//
// A page is brought into a frame when the guest first touches it, from PSRAM,
// or as zeros if it has never been written back. A frame is taken back by
// the clock algorithm: a hand goes round the frames, clearing the RESIDENT
// flag of each page it passes that has been used since, and takes the first
// that has not, writing its page back to PSRAM first if it was written. A
// cleared page used again costs only a call that sets the flag again.
//
// Called from core 0 only.
#ifndef YAX86_PICO_PAGER_H
#define YAX86_PICO_PAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/platform.h"
#include "yax86_pico.h"

enum {
  // A page, and a frame, is 1 << kPagerPageShift bytes: 256. Larger pages
  // spend more of the time in page faults, since a fault moves a whole page
  // and programs use little of a large one, measured in
  // pico/picocalc/AGENTS.md, "Paging guest RAM". Smaller ones need a larger
  // page table, 2 bytes a page: 5KB for 640KB at 256 bytes.
  kPagerPageShift = 8,
  kPagerPageSize = 1 << kPagerPageShift,
};

typedef struct PagerStats {
  uint32_t num_frames;
  // Pages brought into a frame from PSRAM, and as zeros.
  uint32_t num_page_ins;
  uint32_t num_zero_fills;
  // Pages written back to PSRAM on leaving their frame.
  uint32_t num_write_backs;
  // Calls for a page already in its frame, which only set a flag.
  uint32_t num_soft_faults;
  // PSRAM transfers that failed, leaving the page's bytes wrong.
  uint32_t num_errors;
  // Time spent in the page fault handler.
  uint64_t fault_us;
} PagerStats;

// Page size bytes of guest RAM, from the start of the paged region, through
// the frames, which take frames_size bytes, at most kPicoGuestSRAMSize. Returns
// the memory to hand the platform as YPlatformConfig.paged_memory.
YPlatformPagedMemory* PagerInit(
    uint8_t* frames, uint32_t frames_size, uint32_t size);

PagerStats PagerGetStats(void);

#endif  // YAX86_PICO_PAGER_H
