#include "display.h"

#include <stddef.h>
#include <stdio.h>

#include "font_4x10.h"
#include "hardware/sync.h"
#include "lcd.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "runtime_log.h"

enum {
  // The guest's screen: 80 by 25 cells of the 4x10 font, at the top of the
  // panel.
  kGuestScreenColumns = 80,
  kGuestScreenRows = 25,
  kGuestScreenTop = 0,
  kGuestScreenWidth = kGuestScreenColumns * kFont4x10Width,
  kGuestScreenHeight = kGuestScreenRows * kFont4x10Height,

  // The banner and status lines below it, in 4x10 character cells, which take
  // every row the guest's screen leaves.
  kBannerRow = kGuestScreenRows,
  kStatusFirstRow = kBannerRow + 1,
  kStatusLabelColumn = 1,
  kStatusValueColumn = 10,

  // The CGA colors the runtime's own text is drawn in.
  kColorBlack = 0,
  kColorLightGray = 7,
  kColorYellow = 14,
  kColorWhite = 15,

  // How a graphics mode's 640x200 is drawn in the guest's screen's 320x250:
  // two pixels across to one, and four rows down to five.
  kGraphicsColumnsPerPixel = 2,
  kGraphicsRowsIn = 4,
  kGraphicsRowsOut = 5,

  // A screenshot's pixels a reply line, which with the rest of the line keeps
  // it inside the logger's 256 bytes.
  kScreenshotPixelsPerLine = 40,
};

// What core 0 hands core 1, one word through the inter-core FIFO each.
typedef enum DisplayMessage {
  kDisplayMessageFrame = 1,
  kDisplayMessageStatus,
  kDisplayMessageTurnOff,
  kDisplayMessageTurnOn,
  kDisplayMessageScreenshot,
} DisplayMessage;

static const char* const kStatusLabels[kDisplayStatusLineCount] = {
    [kDisplayStatusSystem] = "System",   [kDisplayStatusBoot] = "Boot",
    [kDisplayStatusSpeed] = "Speed",     [kDisplayStatusClock] = "Clock",
    [kDisplayStatusDisplay] = "Display", [kDisplayStatusDisk] = "Disk",
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
// The id of the command a screenshot answers.
static uint32_t g_screenshot_id;
static volatile bool g_screenshot_busy;

// Shared between the cores, so only read or written under the lock.
static critical_section_t g_stats_lock;
static DisplayStats g_stats;

// Core 1's own, while it draws a frame: whether the region being drawn is on
// the panel, and whether it is in a graphics mode.
static bool g_region_on_panel;
static bool g_region_graphics;
static uint16_t g_pixel_batch[Y_VIDEO_PIXEL_BATCH_SIZE];
static uint64_t g_frame_pixels;
// A graphics mode's row, scaled to the panel, as it is put together, and how
// wide it is.
static uint16_t g_graphics_row[kGuestScreenWidth];
static uint16_t g_graphics_row_length;
static uint16_t g_graphics_row_width;

static uint16_t CGAColor(uint8_t index) {
  const YVideoRGB rgb = YVideoDefaultConfig.cga_palette[index];
  return LcdRGB565(rgb.r, rgb.g, rgb.b);
}

// ============================================================================
// Core 1
// ============================================================================

// The first panel row of a graphics mode's row y. A graphics mode renders
// 640x200 - the CGA's 320x200 modes with each pixel doubled - and is drawn at
// half its width and five fourths of its height, which fills the 320x250 the
// guest's text takes. Every fourth row is drawn twice. That comes close to
// the shape a PC/XT's monitor gives the picture: 4:3 is 1.33 times as wide
// as it is high, and 320x250 is 1.28.
static uint16_t GraphicsPanelRow(uint16_t y) {
  return (uint16_t)(y * kGraphicsRowsOut / kGraphicsRowsIn);
}

// The core renders only what changed since the last render, as rectangles,
// and the panel keeps its own frame memory, so a region goes straight to the
// panel as a window of its own.
static void BeginRenderRegion(YVideoState* video, YVideoRegion region) {
  g_region_graphics =
      YVideoGetModeMetadata(video)->type == Y_VIDEO_MODE_GRAPHICS;
  uint16_t x = region.origin.x;
  uint16_t y = region.origin.y;
  uint16_t width = region.width;
  uint16_t height = region.height;
  if (g_region_graphics) {
    x /= kGraphicsColumnsPerPixel;
    width /= kGraphicsColumnsPerPixel;
    y = GraphicsPanelRow(region.origin.y);
    height = (uint16_t)(GraphicsPanelRow(region.origin.y + region.height) - y);
    g_graphics_row_length = 0;
    g_graphics_row_width = width;
  }
  g_region_on_panel =
      x + width <= kGuestScreenWidth && y + height <= kGuestScreenHeight;
  if (g_region_on_panel) {
    LcdBeginWrite(x, (uint16_t)(kGuestScreenTop + y), width, height);
  }
}

// Two neighboring pixels of a graphics mode as one on the panel. In the
// 320x200 modes they are always the same pixel, doubled. In 640x200 they can
// differ, and the panel shows the color between them, so that a line one
// pixel wide is still there, at half its brightness.
static uint16_t GraphicsBlend(YVideoRGB left, YVideoRGB right) {
  return LcdRGB565(
      (uint8_t)((left.r + right.r) / 2), (uint8_t)((left.g + right.g) / 2),
      (uint8_t)((left.b + right.b) / 2));
}

// A graphics mode's pixels are put together a row at a time, since a row
// that is drawn twice has to be sent twice. A region is a whole number of
// the core's dirty columns across, 8 pixels each in a graphics mode, so a
// batch is always an even number of pixels and a pair never spans two.
static void WriteGraphicsPixels(
    YVideoPosition origin, const YVideoRGB* pixels, uint8_t count) {
  for (uint8_t i = 0; i + 1 < count; i += kGraphicsColumnsPerPixel) {
    g_graphics_row[g_graphics_row_length++] =
        GraphicsBlend(pixels[i], pixels[i + 1]);
  }
  if (g_graphics_row_length < g_graphics_row_width) {
    return;
  }
  const uint16_t copies =
      (uint16_t)(GraphicsPanelRow(origin.y + 1) - GraphicsPanelRow(origin.y));
  for (uint16_t i = 0; i < copies; ++i) {
    LcdWritePixels(g_graphics_row, g_graphics_row_width);
  }
  g_frame_pixels += (uint64_t)g_graphics_row_width * copies;
  g_graphics_row_length = 0;
}

static void WriteGuestPixels(
    Y_UNUSED YVideoState* video, YVideoPosition origin, const YVideoRGB* pixels,
    uint8_t count) {
  if (!g_region_on_panel) {
    return;
  }
  if (g_region_graphics) {
    WriteGraphicsPixels(origin, pixels, count);
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

// Read the whole panel back and send it to the host, as the reply to the
// screenshot command: a line with its size, then the pixels, in RGB565 as hex,
// a part of a row a line. Read back rather than drawn again, so that it is
// what the panel holds, however it came to hold it.
static void SendScreenshot(uint32_t id) {
  static const char kCommand[] = "screenshot";
  Y_LOG_REPLY(
      &g_render_logger, id, kCommand, Y_LOG_UINT("width", kLcdWidth),
      Y_LOG_UINT("height", kLcdHeight), Y_LOG_STRING("format", "rgb565"));
  uint16_t pixels[kScreenshotPixelsPerLine];
  // Four hex digits a pixel, and room for the null snprintf() writes after
  // the last.
  char hex[kScreenshotPixelsPerLine * 4 + 1];
  LcdBeginRead(0, 0, kLcdWidth, kLcdHeight);
  for (uint16_t y = 0; y < kLcdHeight; ++y) {
    for (uint16_t x = 0; x < kLcdWidth; x += kScreenshotPixelsPerLine) {
      LcdReadPixels(pixels, kScreenshotPixelsPerLine);
      for (uint8_t i = 0; i < kScreenshotPixelsPerLine; ++i) {
        snprintf(&hex[i * 4], 5, "%04X", pixels[i]);
      }
      Y_LOG_REPLY(
          &g_render_logger, id, kCommand, Y_LOG_UINT("row", y),
          Y_LOG_UINT("x", x), Y_LOG_STRING("pixels", hex));
    }
  }
  LcdEndRead();
  Y_LOG_REPLY(&g_render_logger, id, kCommand, Y_LOG_STRING("status", "ok"));
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
      case kDisplayMessageScreenshot:
        SendScreenshot(g_screenshot_id);
        GiveBack(&g_screenshot_busy);
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
  // The copy is rendered on core 1, so what it logs goes through core 1's own
  // logger rather than the machine's, which core 0 is using.
  g_frame.config.logger = &g_render_logger;
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

bool DisplaySendScreenshot(uint32_t id) {
  if (g_screenshot_busy) {
    return false;
  }
  g_screenshot_id = id;
  HandOver(kDisplayMessageScreenshot, &g_screenshot_busy);
  return true;
}

DisplayStats DisplayGetStats(void) {
  critical_section_enter_blocking(&g_stats_lock);
  const DisplayStats stats = g_stats;
  critical_section_exit(&g_stats_lock);
  return stats;
}
