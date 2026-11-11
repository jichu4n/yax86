#include "pager.h"

#include <string.h>

#include "pico/time.h"
#include "psram.h"

enum {
  // The most guest RAM there can be to page, and so the largest page table.
  kPagerMaxPages = Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE >> kPicoPageShift,
  // The most frames: all of the SRAM guest memory has.
  kPagerMaxFrames = kPicoGuestSRAMSize >> kPicoPageShift,
  kPagerPageSize = 1 << kPicoPageShift,
  // A frame holding no page.
  kPagerNoPage = 0xFFFF,
};

static YPlatformPagedMemory g_memory;
static uint16_t g_pages[kPagerMaxPages];
// The page in each frame, or kPagerNoPage. Whether it has been written since
// it came in is its entry's DIRTY bit, which the hand's sweep leaves alone.
static uint16_t g_frame_pages[kPagerMaxFrames];
// Whether each page has ever been written back, and so whether PSRAM holds
// it. One that has not is zeros, as SRAM guest RAM starts.
static uint8_t g_backed[kPagerMaxPages / 8];
static uint32_t g_num_frames;
static uint32_t g_hand;
static PagerStats g_stats;

static void PagerPageFault(
    YPlatformPagedMemory* memory, uint32_t page, bool write);

YPlatformPagedMemory* PagerInit(
    uint8_t* frames, uint32_t frames_size, uint32_t size) {
  g_num_frames = frames_size >> kPicoPageShift;
  if (g_num_frames > kPagerMaxFrames) {
    g_num_frames = kPagerMaxFrames;
  }
  for (uint32_t i = 0; i < g_num_frames; ++i) {
    g_frame_pages[i] = kPagerNoPage;
  }
  memset(g_pages, 0, sizeof(g_pages));
  memset(g_backed, 0, sizeof(g_backed));
  g_hand = 0;
  memset(&g_stats, 0, sizeof(g_stats));
  g_stats.frames = g_num_frames;
  g_memory.context = NULL;
  g_memory.size = size;
  g_memory.page_shift = kPicoPageShift;
  g_memory.pages = g_pages;
  g_memory.frames = frames;
  g_memory.page_fault_fn = PagerPageFault;
  return &g_memory;
}

static inline uint8_t* PagerFrame(uint32_t frame) {
  return g_memory.frames + (frame << kPicoPageShift);
}

// A frame for a page in none: a free one, or the first the hand comes to
// whose page has not been used since the hand last passed, clearing the
// access of each it passes. Every page has had its access cleared after one
// turn, so it stops within two.
static uint32_t __not_in_flash_func(PagerTakeFrame)(void) {
  for (uint32_t step = 0; step < 2 * g_num_frames; ++step) {
    const uint32_t frame = g_hand;
    g_hand = g_hand + 1 == g_num_frames ? 0 : g_hand + 1;
    const uint16_t page = g_frame_pages[frame];
    if (page == kPagerNoPage) {
      return frame;
    }
    uint16_t* entry = &g_pages[page];
    if (*entry & Y_CPU_PAGED_DATA_RESIDENT) {
      *entry &= (uint16_t)~Y_CPU_PAGED_DATA_RESIDENT;
      continue;
    }
    if (*entry & Y_CPU_PAGED_DATA_DIRTY) {
      if (!PsramWrite(
              page << kPicoPageShift, PagerFrame(frame), kPagerPageSize)) {
        ++g_stats.errors;
      }
      g_backed[page / 8] |= (uint8_t)(1u << (page % 8));
      ++g_stats.write_backs;
    }
    *entry = 0;
    g_frame_pages[frame] = kPagerNoPage;
    return frame;
  }
  // Not reached: the second turn finds every access cleared.
  return 0;
}

static void __not_in_flash_func(PagerPageFault)(
    YPlatformPagedMemory* memory, uint32_t page, bool write) {
  (void)memory;
  const uint32_t start_us = time_us_32();
  uint32_t frame = g_pages[page] & Y_CPU_PAGED_DATA_FRAME_MASK;
  if (frame < g_num_frames && g_frame_pages[frame] == page) {
    ++g_stats.soft_faults;
  } else {
    frame = PagerTakeFrame();
    if (g_frame_pages[frame] != kPagerNoPage) {
      // The fall back out of PagerTakeFrame(), which never happens: the page
      // in the frame has to go somewhere.
      g_pages[g_frame_pages[frame]] = 0;
    }
    if (g_backed[page / 8] & (1u << (page % 8))) {
      if (!PsramRead(
              page << kPicoPageShift, PagerFrame(frame), kPagerPageSize)) {
        ++g_stats.errors;
      }
      ++g_stats.page_ins;
    } else {
      memset(PagerFrame(frame), 0, kPagerPageSize);
      ++g_stats.zero_fills;
    }
    g_frame_pages[frame] = (uint16_t)page;
    g_pages[page] = (uint16_t)frame;
  }
  // A page that stayed in its frame keeps its DIRTY bit; one just brought in
  // has none.
  g_pages[page] |= Y_CPU_PAGED_DATA_RESIDENT;
  if (write) {
    g_pages[page] |= Y_CPU_PAGED_DATA_DIRTY;
  }
  g_stats.fault_us += time_us_32() - start_us;
}

PagerStats PagerGetStats(void) { return g_stats; }
