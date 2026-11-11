#include "pager.h"

#include <string.h>

#include "pico/time.h"
#include "psram.h"

enum {
  // The most guest RAM there can be to page, and so the largest page table.
  kPagerMaxPages = Y_PLATFORM_MAX_PHYSICAL_MEMORY_SIZE >> kPagerPageShift,
  // The most frames: all 128KB of the SRAM guest RAM has.
  kPagerMaxFrames = 128 * 1024 / kPagerPageSize,
  // A frame holding no page.
  kPagerNoPage = 0xFFFF,
  kPagerAccess = Y_CPU_PAGED_DATA_RESIDENT | Y_CPU_PAGED_DATA_WRITABLE,
};

static YPlatformPagedMemory g_memory;
static uint16_t g_pages[kPagerMaxPages];
// The page in each frame, or kPagerNoPage, and whether it has been written
// since it came in.
static uint16_t g_frame_pages[kPagerMaxFrames];
static bool g_frame_dirty[kPagerMaxFrames];
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
  g_num_frames = frames_size >> kPagerPageShift;
  if (g_num_frames > kPagerMaxFrames) {
    g_num_frames = kPagerMaxFrames;
  }
  for (uint32_t i = 0; i < g_num_frames; ++i) {
    g_frame_pages[i] = kPagerNoPage;
    g_frame_dirty[i] = false;
  }
  memset(g_pages, 0, sizeof(g_pages));
  memset(g_backed, 0, sizeof(g_backed));
  g_hand = 0;
  memset(&g_stats, 0, sizeof(g_stats));
  g_stats.frames = g_num_frames;
  g_memory.context = NULL;
  g_memory.size = size;
  g_memory.page_shift = kPagerPageShift;
  g_memory.pages = g_pages;
  g_memory.frames = frames;
  g_memory.page_fault = PagerPageFault;
  return &g_memory;
}

static inline uint8_t* PagerFrame(uint32_t frame) {
  return g_memory.frames + (frame << kPagerPageShift);
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
      *entry &= (uint16_t)~kPagerAccess;
      continue;
    }
    if (g_frame_dirty[frame]) {
      if (!PsramWrite(
              page << kPagerPageShift, PagerFrame(frame), kPagerPageSize)) {
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
              page << kPagerPageShift, PagerFrame(frame), kPagerPageSize)) {
        ++g_stats.errors;
      }
      ++g_stats.page_ins;
    } else {
      memset(PagerFrame(frame), 0, kPagerPageSize);
      ++g_stats.zero_fills;
    }
    g_frame_pages[frame] = (uint16_t)page;
    g_frame_dirty[frame] = false;
  }
  uint16_t entry = (uint16_t)(frame | Y_CPU_PAGED_DATA_RESIDENT);
  if (write) {
    g_frame_dirty[frame] = true;
  }
  // A page already written needs no telling of the next write.
  if (g_frame_dirty[frame]) {
    entry |= Y_CPU_PAGED_DATA_WRITABLE;
  }
  g_pages[page] = entry;
  g_stats.fault_us += time_us_32() - start_us;
}

PagerStats PagerGetStats(void) { return g_stats; }
