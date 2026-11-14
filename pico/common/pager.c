#include "pager.h"

#include <string.h>

#include "pico/time.h"
#include "psram.h"

enum {
  // The most guest RAM there can be to page, and so the largest page table.
  kPagerMaxPages = Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE >> kPagerPageShift,
  // The most frames: all of the SRAM guest memory has.
  kPagerMaxFrames = kPicoGuestSRAMSize >> kPagerPageShift,
  // A frame holding no page.
  kPagerNoPage = 0xFFFF,
};

// The pager's state. Pages, frames, the page table and its RESIDENT and DIRTY
// bits are as the CPU's public.h describes them, above
// Y_CPU_PAGED_DATA_RESIDENT.

// What PagerInit() returns, for the platform.
static YPlatformPagedMemory g_memory;
// The page table, entry i for page i. A page in no frame has entry 0.
static uint16_t g_pages[kPagerMaxPages];
// The reverse of the page table: the page in each frame, or kPagerNoPage.
// An entry's frame number alone cannot say whether the page is in a frame,
// since a page in no frame has entry 0, which names frame 0; this can.
static uint16_t g_frame_pages[kPagerMaxFrames];
// A bit per page: whether it has ever been written back, and so whether PSRAM
// holds its bytes. One that has not is all zeros, as guest RAM starts.
static uint8_t g_backed[kPagerMaxPages / 8];
// How many frames there are, and the clock hand: the next frame
// PagerTakeFrame() looks at.
static uint32_t g_num_frames;
static uint32_t g_hand;
static PagerStats g_stats;

static void PagerPageFault(
    YPlatformPagedMemory* memory, uint32_t page, bool is_write);

YPlatformPagedMemory* PagerInit(
    uint8_t* frames, uint32_t frames_size, uint32_t size) {
  g_num_frames = frames_size >> kPagerPageShift;
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
  g_stats.num_frames = g_num_frames;
  g_memory.context = NULL;
  g_memory.size = size;
  g_memory.page_shift = kPagerPageShift;
  g_memory.pages = g_pages;
  g_memory.frames = frames;
  g_memory.page_fault_fn = PagerPageFault;
  return &g_memory;
}

// The SRAM a frame occupies.
static inline uint8_t* PagerFrame(uint32_t frame) {
  return g_memory.frames + (frame << kPagerPageShift);
}

// Where a page lives in PSRAM: at the same offset as in the paged region.
static inline uint32_t PagerPsramAddress(uint32_t page) {
  return page << kPagerPageShift;
}

static inline bool PagerIsBacked(uint32_t page) {
  return (g_backed[page / 8] & (1u << (page % 8))) != 0;
}

static inline void PagerMarkBacked(uint32_t page) {
  g_backed[page / 8] |= (uint8_t)(1u << (page % 8));
}

// Choose a frame for a page that is in none, and empty it. Returns a frame
// that holds no page.
//
// This is the clock algorithm. The frames form a circle, and the hand moves
// round it a frame at a time. At each frame:
//
//   - A free frame is taken at once. Frames are free only until the first
//     turn has filled them all.
//   - A page with RESIDENT set has been used since the hand last came by,
//     because using a page needs RESIDENT, and a page without it gets it back
//     from PagerPageFault() on its next use. The hand clears RESIDENT, giving
//     the page until the hand comes round again to be used, and moves on.
//   - A page with RESIDENT clear has not been used for a whole turn of the
//     hand. Its frame is taken: the page is written back to PSRAM if DIRTY
//     says the frame differs from it, and its entry is cleared.
//
// The loop ends within two turns: the first clears RESIDENT on every page it
// passes, and nothing sets it again while the loop runs, so the second turn
// takes the first frame it reaches.
static uint32_t __not_in_flash_func(PagerTakeFrame)(void) {
  for (;;) {
    // Look at the hand's frame, and move the hand on, wrapping at the end.
    const uint32_t frame = g_hand;
    g_hand = g_hand + 1 == g_num_frames ? 0 : g_hand + 1;

    const uint16_t page = g_frame_pages[frame];
    if (page == kPagerNoPage) {
      return frame;
    }

    uint16_t* entry = &g_pages[page];
    if (*entry & Y_CPU_PAGED_DATA_RESIDENT) {
      // Used recently: give it another turn. The CPU's next access to it
      // faults, and PagerPageFault() sets RESIDENT again without moving it.
      *entry &= (uint16_t)~Y_CPU_PAGED_DATA_RESIDENT;
      continue;
    }

    // Not used for a turn: evict it. Only a page written since it came in
    // needs writing back; otherwise PSRAM, or zeros, already hold its bytes.
    if (*entry & Y_CPU_PAGED_DATA_DIRTY) {
      if (!PsramWrite(
              PagerPsramAddress(page), PagerFrame(frame), kPagerPageSize)) {
        ++g_stats.num_errors;
      }
      PagerMarkBacked(page);
      ++g_stats.num_write_backs;
    }
    *entry = 0;
    g_frame_pages[frame] = kPagerNoPage;
    return frame;
  }
}

// The page fault handler: YPlatformPagedMemory.page_fault_fn, called when the
// CPU, or anything else reading or writing guest RAM, needs a page its entry
// does not allow. page is the page's number, and is_write is true for a
// write. It must leave the entry allowing the access: RESIDENT for a read,
// and RESIDENT and DIRTY for a write.
//
// There are three cases:
//
//   - Hard fault: the page is in no frame. A frame is taken from another page
//     (PagerTakeFrame()), and the page is read into it from PSRAM, or zeroed
//     if it has never been written back.
//   - Soft fault after sampling: the page is still in its frame, and the
//     clock hand cleared RESIDENT to see whether it is still used. It is, so
//     RESIDENT is set again.
//   - Soft fault on a first write: the page is in its frame with RESIDENT set
//     but DIRTY clear, and this is its first write since it came in. DIRTY is
//     set, which marks it for writing back when it leaves the frame.
//
// A soft fault moves no data; it only sets bits.
static void __not_in_flash_func(PagerPageFault)(
    YPlatformPagedMemory* memory, uint32_t page, bool is_write) {
  (void)memory;
  const uint32_t start_us = time_us_32();

  // Whether the page is in a frame. Its entry names a frame even when it is
  // in none, since entry 0 names frame 0, so the frame's own record decides.
  uint32_t frame = g_pages[page] & Y_CPU_PAGED_DATA_FRAME_MASK;
  const bool is_in_frame = frame < g_num_frames && g_frame_pages[frame] == page;

  if (is_in_frame) {
    // A soft fault: the entry keeps its frame and its DIRTY bit, and only
    // gains the bits below.
    ++g_stats.num_soft_faults;
  } else {
    // A hard fault: bring the page into a frame of its own.
    frame = PagerTakeFrame();
    if (PagerIsBacked(page)) {
      if (!PsramRead(
              PagerPsramAddress(page), PagerFrame(frame), kPagerPageSize)) {
        ++g_stats.num_errors;
      }
      ++g_stats.num_page_ins;
    } else {
      memset(PagerFrame(frame), 0, kPagerPageSize);
      ++g_stats.num_zero_fills;
    }
    // Record the page in the frame both ways. The entry starts with neither
    // bit: the frame matches PSRAM until it is written.
    g_frame_pages[frame] = (uint16_t)page;
    g_pages[page] = (uint16_t)frame;
  }

  // Allow the access. A write sets DIRTY too, so that the CPU makes the
  // page's later writes in its frame without calling here, and so that the
  // page is written back when it leaves the frame.
  g_pages[page] |= Y_CPU_PAGED_DATA_RESIDENT;
  if (is_write) {
    g_pages[page] |= Y_CPU_PAGED_DATA_DIRTY;
  }
  g_stats.fault_us += time_us_32() - start_us;
}

PagerStats PagerGetStats(void) { return g_stats; }
