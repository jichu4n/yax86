#include "display.h"

#include <stddef.h>

#include "font_4x10.h"
#include "hardware/sync.h"
#include "lcd.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "pico/time.h"

enum {
  // The guest's screen: 80 by 25 cells of the 4x10 font, at the top of the
  // panel.
  kGuestScreenColumns = 80,
  kGuestScreenRows = 25,
  kGuestScreenTop = 0,
  kGuestScreenWidth = kGuestScreenColumns * kFont4x10Width,
  kGuestScreenHeight = kGuestScreenRows * kFont4x10Height,

  // The banner and status lines below it, in 4x10 character cells. The row
  // after the guest's screen is left blank to separate them from it.
  kBannerRow = kGuestScreenRows + 1,
  kStatusFirstRow = kBannerRow + 1,
  kStatusLabelColumn = 1,
  kStatusValueColumn = 10,

  // The CGA colors the runtime's own text is drawn in.
  kColorBlack = 0,
  kColorLightGray = 7,
  kColorYellow = 14,
  kColorWhite = 15,
};

// What core 0 hands core 1, one word through the inter-core FIFO each.
typedef enum DisplayMessage {
  kDisplayMessageFrame = 1,
  kDisplayMessageStatus,
  kDisplayMessageTurnOff,
  kDisplayMessageTurnOn,
} DisplayMessage;

static const char* const kStatusLabels[kDisplayStatusLineCount] = {
    [kDisplayStatusSystem] = "System",   [kDisplayStatusBoot] = "Boot",
    [kDisplayStatusSpeed] = "Speed",     [kDisplayStatusClock] = "Clock",
    [kDisplayStatusDisplay] = "Display",
};

// What core 0 hands over. Each has a flag that only core 0 sets, once it has
// filled the buffer in, and only core 1 clears, once it has drawn it. While it
// is set only core 1 uses the buffer, and while it is clear only core 0 does,
// so neither ever waits for the other: core 0 finding it set leaves the
// buffer alone and tries again later.
static YVideoState g_frame;
static uint8_t g_frame_vram[Y_VIDEO_CGA_VRAM_SIZE];
static volatile bool g_frame_busy;
static DisplayStatus g_status;
static volatile bool g_status_busy;

// Shared between the cores, so only read or written under the lock.
static critical_section_t g_stats_lock;
static DisplayStats g_stats;

// Core 1's own, while it draws a frame.
static bool g_region_on_panel;
static uint16_t g_pixel_batch[Y_VIDEO_PIXEL_BATCH_SIZE];
static uint64_t g_frame_pixels;

static uint16_t CGAColor(uint8_t index) {
  const YVideoRGB rgb = YVideoDefaultConfig.cga_palette[index];
  return LcdRGB565(rgb.r, rgb.g, rgb.b);
}

// ============================================================================
// Core 1
// ============================================================================

// The core renders only what changed since the last render, as rectangles,
// and the panel keeps its own frame memory, so a region goes straight to the
// panel as a window of its own.
static void BeginRenderRegion(
    Y_UNUSED YVideoState* video, YVideoRegion region) {
  g_region_on_panel = region.origin.x + region.width <= kGuestScreenWidth &&
                      region.origin.y + region.height <= kGuestScreenHeight;
  if (g_region_on_panel) {
    LcdBeginWrite(
        region.origin.x, (uint16_t)(kGuestScreenTop + region.origin.y),
        region.width, region.height);
  }
}

// Graphics modes render 640 pixels across, which is not drawn yet.
static void WriteGuestPixels(
    Y_UNUSED YVideoState* video, Y_UNUSED YVideoPosition origin,
    const YVideoRGB* pixels, uint8_t count) {
  if (!g_region_on_panel) {
    return;
  }
  for (uint8_t i = 0; i < count; ++i) {
    g_pixel_batch[i] = LcdRGB565(pixels[i].r, pixels[i].g, pixels[i].b);
  }
  LcdWritePixels(g_pixel_batch, count);
  g_frame_pixels += count;
}

static void EndRenderRegion(Y_UNUSED YVideoState* video) {
  if (g_region_on_panel) {
    LcdEndWrite();
  }
}

static void DrawFrame(void) {
  g_frame_pixels = 0;
  const uint64_t start_us = time_us_64();
  YVideoRender(&g_frame);
  const uint64_t render_us = time_us_64() - start_us;
  critical_section_enter_blocking(&g_stats_lock);
  g_stats.render_us += render_us;
  g_stats.pixels_drawn += g_frame_pixels;
  if (g_frame_pixels > 0) {
    ++g_stats.frames_drawn;
  }
  critical_section_exit(&g_stats_lock);
}

// Draw one status line, padded out to the right edge so that a shorter value
// covers a longer one.
static void DrawStatusLine(DisplayStatusLine line, const char* value) {
  char padded[kTextColumns - kStatusValueColumn + 1];
  size_t length = 0;
  while (value[length] != '\0' && length < sizeof(padded) - 1) {
    padded[length] = value[length];
    ++length;
  }
  while (length < sizeof(padded) - 1) {
    padded[length++] = ' ';
  }
  padded[length] = '\0';
  const uint8_t row = (uint8_t)(kStatusFirstRow + line);
  TextDrawString(
      kStatusLabelColumn, row, kStatusLabels[line], CGAColor(kColorLightGray),
      CGAColor(kColorBlack));
  TextDrawString(
      kStatusValueColumn, row, padded, CGAColor(kColorWhite),
      CGAColor(kColorBlack));
}

static void DrawStatus(void) {
  for (uint8_t line = 0; line < kDisplayStatusLineCount; ++line) {
    DrawStatusLine((DisplayStatusLine)line, g_status.values[line]);
  }
}

// Mark a buffer core 0 handed over as drawn, giving it back.
static void GiveBack(volatile bool* busy) {
  __dmb();
  *busy = false;
}

static void DisplayMain(void) {
  const uint32_t lcd_hz = LcdInit();
  TextDrawString(
      kStatusLabelColumn, kBannerRow,
      "yax86 - an IBM PC/XT emulator, on the ClockworkPi PicoCalc",
      CGAColor(kColorYellow), CGAColor(kColorBlack));
  multicore_fifo_push_blocking(lcd_hz);

  for (;;) {
    const uint32_t message = multicore_fifo_pop_blocking();
    __dmb();
    switch (message) {
      case kDisplayMessageFrame:
        DrawFrame();
        GiveBack(&g_frame_busy);
        break;
      case kDisplayMessageStatus:
        DrawStatus();
        GiveBack(&g_status_busy);
        break;
      case kDisplayMessageTurnOff:
        LcdSetDisplayOn(false);
        break;
      case kDisplayMessageTurnOn:
        LcdSetDisplayOn(true);
        break;
    }
  }
}

// ============================================================================
// Core 0
// ============================================================================

// Hand core 1 a buffer core 0 has just filled in. There is never more than one
// message of each kind outstanding, so the FIFO, eight deep, never fills and
// this never waits.
static void HandOver(DisplayMessage message, volatile bool* busy) {
  *busy = true;
  __dmb();
  multicore_fifo_push_blocking(message);
}

uint32_t DisplayStart(void) {
  critical_section_init(&g_stats_lock);
  multicore_launch_core1(DisplayMain);
  return multicore_fifo_pop_blocking();
}

void DisplayOfferFrame(YVideoState* video) {
  if (video->dirty_state.status == Y_VIDEO_CLEAN) {
    return;
  }
  if (g_frame_busy) {
    critical_section_enter_blocking(&g_stats_lock);
    ++g_stats.frames_late;
    critical_section_exit(&g_stats_lock);
    return;
  }
  const uint64_t start_us = time_us_64();
  YVideoCopyForRender(video, &g_frame, g_frame_vram);
  g_frame.config.begin_render_region = BeginRenderRegion;
  g_frame.config.write_pixels = WriteGuestPixels;
  g_frame.config.end_render_region = EndRenderRegion;
  const uint64_t copy_us = time_us_64() - start_us;
  critical_section_enter_blocking(&g_stats_lock);
  g_stats.copy_us += copy_us;
  critical_section_exit(&g_stats_lock);
  HandOver(kDisplayMessageFrame, &g_frame_busy);
}

DisplayStatus* DisplayBeginStatus(void) {
  return g_status_busy ? NULL : &g_status;
}

void DisplayEndStatus(void) { HandOver(kDisplayMessageStatus, &g_status_busy); }

void DisplayTurnOff(void) {
  __dmb();
  multicore_fifo_push_blocking(kDisplayMessageTurnOff);
}

void DisplayTurnOn(void) {
  __dmb();
  multicore_fifo_push_blocking(kDisplayMessageTurnOn);
}

DisplayStats DisplayGetStats(void) {
  critical_section_enter_blocking(&g_stats_lock);
  const DisplayStats stats = g_stats;
  critical_section_exit(&g_stats_lock);
  return stats;
}
