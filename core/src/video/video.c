#ifndef YAX86_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_VIDEO_LOG(level, ...) \
  YAX86_LOG(video->config->logger, &kLogModuleVideo, level, __VA_ARGS__)

// Power-on 6845 register values for the IBM Monochrome Display.
static const uint8_t kDefaultMDARegisters[kNumCRTCRegisters] = {
    0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
    0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Power-on 6845 register values for the CGA in 80x25 text mode, matching the
// values GLaBIOS programs for that mode.
static const uint8_t kDefaultCGARegisters[kNumCRTCRegisters] = {
    0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02,
    0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // Value of bits 6-5 of the cursor start register that disables the cursor.
  kCRTCCursorDisabled = 0x20,
  // Mask of bits 6-5 of the cursor start register.
  kCRTCCursorModeMask = 0x60,
  // Mask of the scan line select bits in the cursor start and end registers.
  kCRTCCursorScanLineMask = 0x1F,

  // The 6845 latches only the low five bits of the register index.
  kCRTCRegisterIndexMask = 0x1F,

  // Value returned for reads that decode to nothing.
  kVideoUnmappedPortValue = 0xFF,

  kVideoTextColumns40 = 40,
  kVideoTextColumns80 = 80,
  kVideoText40ColumnScale = 2,
  kVideoText80ColumnScale = 1,
};

static void VideoInvalidateVRAMAddress(VideoState* video, uint32_t address);
static void VideoInvalidateCursor(VideoState* video);
static void VideoInvalidateBlinkingText(VideoState* video);
static void VideoInvalidateAll(VideoState* video);

const VideoAdapterMetadata* VideoGetAdapterMetadata(const VideoState* video) {
  return &kVideoAdapterMetadata[video->adapter];
}

// ============================================================================
// Video RAM
// ============================================================================

YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address) {
  if (!video->config || !video->config->vram) {
    return kVideoUnmappedPortValue;
  }
  // VRAM is aliased throughout the adapter's window, so an address past the end
  // wraps around rather than reading nothing. The renderer relies on this when
  // the 6845 start address pushes it past the end of VRAM.
  //
  // Every adapter's VRAM size is a power of two, so the wrap is a mask rather
  // than a remainder. The size is only known at run time, so a remainder would
  // compile to a hardware divide in the middle of the render loop - and the
  // Cortex-M0+ this targets has no divide instruction at all.
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  return video->config->vram[address & (vram_size - 1)];
}

YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (!video->config || !video->config->vram) {
    return;
  }
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  address &= vram_size - 1;
  if (video->config->vram[address] == value) {
    return;
  }
  video->config->vram[address] = value;
  if (video->dirty_state.status == kVideoFullRedraw) {
    return;
  }
  VideoInvalidateVRAMAddress(video, address);
}

uint8_t VideoReadVRAM(VideoState* video, uint32_t address) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return kVideoUnmappedPortValue;
  }
  return VideoReadVRAMByte(video, address);
}

void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return;
  }
  VideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

void VideoInit(VideoState* video, VideoConfig* config) {
  static const VideoState kEmptyVideoState = {0};
  *video = kEmptyVideoState;
  video->config = config;
  video->adapter = config && config->adapter == kVideoAdapterCGA
                       ? kVideoAdapterCGA
                       : kVideoAdapterMDA;

  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  const uint8_t* default_registers = video->adapter == kVideoAdapterCGA
                                         ? kDefaultCGARegisters
                                         : kDefaultMDARegisters;
  for (uint8_t i = 0; i < kNumCRTCRegisters; ++i) {
    video->registers[i] = default_registers[i];
  }
  video->control_register = adapter->default_control_register;
  VideoInvalidateAll(video);

  for (uint32_t i = 0; i < adapter->vram_size; i += 2) {
    VideoWriteVRAMByte(video, i, ' ');
    VideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// Video mode
// ============================================================================

VideoMode VideoGetMode(const VideoState* video) {
  if (video->adapter == kVideoAdapterMDA) {
    // The MDA has only one mode.
    return kVideoModeMDAText80x25;
  }
  // Derive the CGA mode from the mode control register, matching the values
  // the BIOS writes for each of its INT 10h modes.
  //
  // The bits are tested in the order a real card resolves them: the high
  // resolution bit wins over the graphics bits, and the graphics bit over the
  // high resolution graphics bit. The BIOS never writes a combination where
  // the order matters, but software that ORs a bit into a saved mode byte can,
  // and this is the order 86Box's renderer decodes them in.
  if (video->control_register & kVideoControlHighResolution) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText80x25Mono
               : kVideoModeCGAText80x25Color;
  }
  if (!(video->control_register & kVideoControlGraphics)) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText40x25Mono
               : kVideoModeCGAText40x25Color;
  }
  if (video->control_register & kVideoControlHighResolutionGraphics) {
    return kVideoModeCGAGraphics640x200;
  }
  return video->control_register & kVideoControlBlackAndWhite
             ? kVideoModeCGAGraphics320x200Alt
             : kVideoModeCGAGraphics320x200;
}

const VideoModeMetadata* VideoGetModeMetadata(const VideoState* video) {
  return &kVideoModeMetadata[VideoGetMode(video)];
}

// ============================================================================
// 6845 CRT controller
// ============================================================================

YAX86_PRIVATE uint16_t VideoGetStartAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterStartAddressH] << 8) |
         video->registers[kCRTCRegisterStartAddressL];
}

YAX86_PRIVATE uint16_t VideoGetCursorAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterCursorH] << 8) |
         video->registers[kCRTCRegisterCursorL];
}

YAX86_PRIVATE bool VideoIsCursorEnabled(const VideoState* video) {
  return (video->registers[kCRTCRegisterCursorStart] & kCRTCCursorModeMask) !=
         kCRTCCursorDisabled;
}

YAX86_PRIVATE uint8_t VideoGetCursorStartScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorStart] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE uint8_t VideoGetCursorEndScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorEnd] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE bool VideoIsCursorBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerCursorBlinkPhase) % 2 == 0;
}

YAX86_PRIVATE bool VideoIsTextBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerTextBlinkPhase) % 2 == 0;
}

YAX86_PRIVATE bool VideoGetVisibleCursorOffset(
    const VideoState* video, const VideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset) {
  if (!VideoIsCursorEnabled(video) || !VideoIsCursorBlinkOn(video)) {
    return false;
  }
  *cursor_offset = (VideoGetCursorAddress(video) - start_address) &
                   (metadata->vram_size / 2 - 1);
  return *cursor_offset < (uint16_t)metadata->columns * metadata->rows;
}

// ============================================================================
// Dirty regions
// ============================================================================

static void VideoDirtyRangeAdd(
    VideoDirtyRange* range, uint8_t start_column, uint8_t end_column) {
  if (range->end_column == 0) {
    range->start_column = start_column;
    range->end_column = end_column;
    return;
  }
  if (start_column < range->start_column) {
    range->start_column = start_column;
  }
  if (end_column > range->end_column) {
    range->end_column = end_column;
  }
}

// The vertical unit of dirty tracking for the current mode.
typedef struct VideoDirtyGeometry {
  // Number of dirty rows covering the display.
  uint8_t rows;
  // Physical scan lines per dirty row.
  uint8_t scan_lines_per_row;
} VideoDirtyGeometry;

static VideoDirtyGeometry VideoGetDirtyGeometry(const VideoState* video) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  VideoDirtyGeometry geometry;
  if (metadata->type == kVideoModeText) {
    geometry.rows = metadata->rows;
    geometry.scan_lines_per_row = metadata->char_height;
  } else {
    geometry.rows =
        (uint8_t)((metadata->height + kVideoDirtyScanLinesPerGroup - 1) /
                  kVideoDirtyScanLinesPerGroup);
    geometry.scan_lines_per_row = kVideoDirtyScanLinesPerGroup;
  }
  if (geometry.rows > kVideoDirtyRowCount) {
    geometry.rows = kVideoDirtyRowCount;
  }
  return geometry;
}

YAX86_PRIVATE void VideoInvalidateRows(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row) {
  if (start_column >= end_column || end_column > kVideoDirtyColumns ||
      first_row >= end_row || first_row >= kVideoDirtyRowCount) {
    return;
  }
  if (end_row > kVideoDirtyRowCount) {
    end_row = kVideoDirtyRowCount;
  }

  for (uint8_t row = first_row; row < end_row; ++row) {
    VideoDirtyRangeAdd(
        &video->dirty_state.ranges[row], start_column, end_column);
  }
  // A pending full redraw already covers these rows, and downgrading it would
  // drop the rest of the frame.
  if (video->dirty_state.status == kVideoClean) {
    video->dirty_state.status = kVideoDirty;
  }
}

static void VideoInvalidateAll(VideoState* video) {
  video->dirty_state.status = kVideoFullRedraw;
}

static void VideoInvalidateTextCell(
    VideoState* video, const VideoModeMetadata* metadata,
    uint16_t cell_offset) {
  if (cell_offset >= (uint16_t)metadata->columns * metadata->rows) {
    return;
  }
  uint8_t row;
  uint8_t col;
  uint8_t column_scale;
  if (metadata->columns == kVideoTextColumns40) {
    row = (uint8_t)(cell_offset / kVideoTextColumns40);
    col = (uint8_t)(cell_offset % kVideoTextColumns40);
    column_scale = kVideoText40ColumnScale;
  } else {
    row = (uint8_t)(cell_offset / kVideoTextColumns80);
    col = (uint8_t)(cell_offset % kVideoTextColumns80);
    column_scale = kVideoText80ColumnScale;
  }
  // The character row is the dirty row, so a changed cell marks exactly the
  // scan lines it covers - no conversion to scan lines and back.
  VideoInvalidateRows(
      video, col * column_scale, (col + 1) * column_scale, row,
      (uint8_t)(row + 1));
}

static void VideoInvalidateVRAMAddress(VideoState* video, uint32_t address) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  if (metadata->type == kVideoModeText) {
    uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
    uint16_t character_address = (uint16_t)(address / 2);
    uint16_t cell_offset =
        (character_address - VideoGetStartAddress(video)) & character_mask;
    VideoInvalidateTextCell(video, metadata, cell_offset);
    return;
  }

  // Each half of CGA graphics VRAM holds either the even or odd scan lines.
  uint32_t half_mask = kCGAGraphicsOddScanLineOffset - 1;
  uint32_t start = (uint32_t)VideoGetStartAddress(video) * 2 & half_mask;
  uint32_t offset = ((address & half_mask) - start) & half_mask;
  uint32_t visible_bytes =
      (metadata->height / 2) * kCGAGraphicsBytesPerScanLine;
  if (offset >= visible_bytes) {
    return;
  }
  uint16_t row_in_half = (uint16_t)(offset / kCGAGraphicsBytesPerScanLine);
  uint8_t byte_column = offset % kCGAGraphicsBytesPerScanLine;
  uint16_t y =
      row_in_half * 2 + (address >= kCGAGraphicsOddScanLineOffset ? 1 : 0);
  // Graphics modes have no character rows, so neighboring scan lines share a
  // dirty row.
  uint8_t dirty_row = (uint8_t)(y / kVideoDirtyScanLinesPerGroup);
  VideoInvalidateRows(
      video, byte_column, byte_column + 1, dirty_row, (uint8_t)(dirty_row + 1));
}

static void VideoInvalidateCursor(VideoState* video) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  if (metadata->type != kVideoModeText) {
    return;
  }
  uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
  uint16_t cursor_offset =
      (VideoGetCursorAddress(video) - VideoGetStartAddress(video)) &
      character_mask;
  VideoInvalidateTextCell(video, metadata, cursor_offset);
}

static void VideoInvalidateBlinkingText(VideoState* video) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  if (metadata->type != kVideoModeText ||
      !(video->control_register & kVideoControlEnableBlink)) {
    return;
  }

  uint16_t start_address = VideoGetStartAddress(video);
  uint8_t column_scale = metadata->columns == kVideoTextColumns40
                             ? kVideoText40ColumnScale
                             : kVideoText80ColumnScale;
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    uint8_t start_column = kVideoDirtyColumns;
    uint8_t end_column = 0;
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t attr_address = ((uint32_t)start_address + cell_offset) * 2 + 1;
      if (VideoReadVRAMByte(video, attr_address) & kVideoAttributeBlink) {
        uint8_t dirty_column = col * column_scale;
        if (dirty_column < start_column) {
          start_column = dirty_column;
        }
        end_column = (col + 1) * column_scale;
      }
    }
    if (end_column != 0) {
      VideoInvalidateRows(
          video, start_column, end_column, row, (uint8_t)(row + 1));
    }
  }
}

// ============================================================================
// Retrace timing
// ============================================================================

// Advances a software model of the CRT beam position by the given number of
// CPU cycles. This is what status port reads (below) derive retrace timing
// from.
//
// The beam advances one scan line every kMDACyclesPerScanLine cycles, but
// callers report cycles in whatever amount the CPU just consumed, which
// rarely divides evenly. scan_line_cycles banks the remainder as credit
// toward the next scan line; the loop pays it off a scan line at a time,
// looping rather than branching once since a single call can be worth more
// than one scan line. scan_line wraps at the adapter's scan_lines_per_frame,
// incrementing frames, which VideoIsCursorBlinkOn() and VideoIsTextBlinkOn()
// use to derive their blink phases.
void VideoTick(VideoState* video, uint32_t cycles) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  if (adapter->cycles_per_scan_line == 0 ||
      adapter->scan_lines_per_frame == 0) {
    // No adapter geometry to advance through.
    return;
  }

  bool cursor_was_on = VideoIsCursorBlinkOn(video);
  bool text_was_on = VideoIsTextBlinkOn(video);

  // Computed rather than stepped, because the platform advances the beam in
  // arrears - only when something actually looks at it - so cycles here can be
  // a whole frame's worth rather than a single instruction's.
  const uint32_t total = video->scan_line_cycles + cycles;
  video->scan_line_cycles = total % adapter->cycles_per_scan_line;

  const uint32_t elapsed_scan_lines = total / adapter->cycles_per_scan_line;
  if (elapsed_scan_lines == 0) {
    return;
  }
  const uint32_t scan_line = (uint32_t)video->scan_line + elapsed_scan_lines;
  video->frames += scan_line / adapter->scan_lines_per_frame;
  video->scan_line = (uint16_t)(scan_line % adapter->scan_lines_per_frame);

  if (cursor_was_on != VideoIsCursorBlinkOn(video) &&
      VideoIsCursorEnabled(video)) {
    VideoInvalidateCursor(video);
  }
  if (text_was_on != VideoIsTextBlinkOn(video)) {
    VideoInvalidateBlinkingText(video);
  }
}

// The value the status port reads back, computed from where the CRT beam
// currently is.
static uint8_t VideoGetStatus(const VideoState* video) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  bool in_vertical_retrace = video->scan_line >= adapter->displayed_scan_lines;
  bool in_horizontal_retrace =
      video->scan_line_cycles >= adapter->display_cycles_per_scan_line;

  // No light pen is emulated, so its switch always reads as off.
  uint8_t status = kVideoStatusLightPenSwitchOff;
  if (in_vertical_retrace || in_horizontal_retrace) {
    status |= kVideoStatusDisplayDisabled;
  }
  if (in_vertical_retrace) {
    status |= kVideoStatusVerticalRetrace;
  }
  // Note that the unused high bits are deliberately left clear rather than
  // floating high as they do on a real card: GLaBIOS probes bit 7 of the MDA
  // status port to detect a Hercules adapter.
  return status;
}

// ============================================================================
// I/O ports
// ============================================================================

// What an I/O port in the adapter's range does.
typedef enum VideoPortFunction {
  // Port is not decoded by the adapter.
  kVideoPortNone = 0,
  // 6845 register index.
  kVideoPortRegisterIndex,
  // 6845 register data.
  kVideoPortRegisterData,
  // Mode control register.
  kVideoPortControl,
  // CGA color select register.
  kVideoPortColorSelect,
  // Status register.
  kVideoPortStatus,
} VideoPortFunction;

// Decode an I/O port within the adapter's range.
static VideoPortFunction VideoDecodePort(
    const VideoState* video, uint16_t port) {
  if (video->adapter == kVideoAdapterMDA) {
    switch (port) {
      case kMDAPortRegisterIndex:
        return kVideoPortRegisterIndex;
      case kMDAPortRegisterData:
        return kVideoPortRegisterData;
      case kMDAPortControl:
        return kVideoPortControl;
      case kMDAPortStatus:
        return kVideoPortStatus;
      default:
        return kVideoPortNone;
    }
  }

  // The CGA only decodes the low four address bits, so the 6845 index and data
  // registers are aliased across 3D0 to 3D7.
  uint16_t offset = port - kCGAPortStart;
  if (offset < 8) {
    return offset & 1 ? kVideoPortRegisterData : kVideoPortRegisterIndex;
  }
  switch (port) {
    case kCGAPortControl:
      return kVideoPortControl;
    case kCGAPortColorSelect:
      return kVideoPortColorSelect;
    case kCGAPortStatus:
      return kVideoPortStatus;
    default:
      // The light pen strobe ports respond but do nothing, as no light pen is
      // emulated.
      return kVideoPortNone;
  }
}

uint8_t VideoReadPort(VideoState* video, uint16_t port) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      return video->selected_register;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        return video->registers[video->selected_register];
      }
      return kVideoUnmappedPortValue;
    case kVideoPortControl:
      return video->control_register;
    case kVideoPortColorSelect:
      return video->color_select_register;
    case kVideoPortStatus:
      return VideoGetStatus(video);
    default:
      return kVideoUnmappedPortValue;
  }
}

void VideoWritePort(VideoState* video, uint16_t port, uint8_t value) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      video->selected_register = value & kCRTCRegisterIndexMask;
      break;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        uint8_t* selected = &video->registers[video->selected_register];
        if (*selected == value) {
          break;
        }
        if (video->selected_register == kCRTCRegisterCursorH ||
            video->selected_register == kCRTCRegisterCursorL) {
          VideoInvalidateCursor(video);
          *selected = value;
          VideoInvalidateCursor(video);
        } else {
          *selected = value;
          if (video->selected_register == kCRTCRegisterStartAddressH ||
              video->selected_register == kCRTCRegisterStartAddressL) {
            VideoInvalidateAll(video);
          } else if (
              video->selected_register == kCRTCRegisterCursorStart ||
              video->selected_register == kCRTCRegisterCursorEnd) {
            VideoInvalidateCursor(video);
          }
        }
      }
      break;
    case kVideoPortControl:
      if (video->control_register != value) {
        video->control_register = value;
        VideoInvalidateAll(video);
      }
      break;
    case kVideoPortColorSelect:
      if (video->color_select_register != value) {
        video->color_select_register = value;
        VideoInvalidateAll(video);
      }
      break;
    default:
      // The status port is read only.
      break;
  }
}

// ============================================================================
// Rendering
// ============================================================================

static void VideoRenderBlankRegion(
    VideoState* video, VideoPixelRun* run, VideoRegion region) {
  RGB blank = video->adapter == kVideoAdapterCGA ? video->config->cga_palette[0]
                                                 : video->config->background;
  uint16_t end_x = region.origin.x + region.width;
  uint16_t end_y = region.origin.y + region.height;
  for (uint16_t y = region.origin.y; y < end_y; ++y) {
    VideoPixelRunBegin(run, region.origin.x, y);
    for (uint16_t x = region.origin.x; x < end_x; ++x) {
      VideoPixelRunPush(run, blank);
    }
  }
}

static void VideoRenderDirtyRegion(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  // Every mode divides the frame buffer into kVideoDirtyColumns equal columns,
  // so a dirty column is 9 pixels wide on the MDA and 8 on the CGA.
  uint16_t column_width =
      VideoGetAdapterMetadata(video)->frame_buffer_width / kVideoDirtyColumns;
  VideoRegion region = {
      .origin = {.x = start_column * column_width, .y = first_y},
      .width = (end_column - start_column) * column_width,
      .height = end_y - first_y,
  };
  if (video->config->begin_render_region) {
    video->config->begin_render_region(video, region);
  }

  video->num_pixels_emitted_for_region = 0;
  // Only these two members need a value on the way in: origin is set whenever
  // a span starts, and a pixel is stored before the count that exposes it to
  // the flush. Initializing the whole struct would clear the 96-byte batch on
  // every region, which a graphics frame can split into fifty of them.
  VideoPixelRun run;
  run.video = video;
  run.count = 0;

  if (!(video->control_register & kVideoControlVideoEnable)) {
    VideoRenderBlankRegion(video, &run, region);
  } else if (video->adapter == kVideoAdapterCGA) {
    CGARenderRegion(video, &run, start_column, end_column, first_y, end_y);
  } else {
    MDARenderRegion(video, &run, start_column, end_column, first_y, end_y);
  }
  VideoPixelRunFlush(&run);

  // A retained display addressed by transfer window advances its own write
  // pointer once per pixel, so it is the pixel count that positions them, not
  // the coordinates passed alongside. Emitting a different number than the
  // region declared leaves that pointer mid-window and displaces every pixel
  // after it, including those of later regions - so report the region that
  // caused it rather than let the corruption surface somewhere else.
  uint32_t declared_pixels = (uint32_t)region.width * region.height;
  if (video->num_pixels_emitted_for_region != declared_pixels) {
    YAX86_VIDEO_LOG(
        kLogLevelError,
        "render region %ux%u at (%u,%u) emitted %u pixels, expected %u",
        region.width, region.height, region.origin.x, region.origin.y,
        video->num_pixels_emitted_for_region, declared_pixels);
  }

  if (video->config->end_render_region) {
    video->config->end_render_region(video);
  }
}

void VideoRender(VideoState* video) {
  if (!video->config || !video->config->write_pixels ||
      video->dirty_state.status == kVideoClean) {
    return;
  }

  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  if (video->dirty_state.status == kVideoFullRedraw) {
    VideoRenderDirtyRegion(
        video, 0, kVideoDirtyColumns, 0, adapter->frame_buffer_height);
  } else {
    VideoDirtyGeometry geometry = VideoGetDirtyGeometry(video);
    for (uint8_t row = 0; row < geometry.rows;) {
      VideoDirtyRange range = video->dirty_state.ranges[row];
      if (range.end_column == 0) {
        ++row;
        continue;
      }

      uint8_t end_row = row + 1;
      while (end_row < geometry.rows &&
             video->dirty_state.ranges[end_row].start_column ==
                 range.start_column &&
             video->dirty_state.ranges[end_row].end_column ==
                 range.end_column) {
        ++end_row;
      }
      uint16_t first_y = (uint16_t)row * geometry.scan_lines_per_row;
      uint16_t end_y = (uint16_t)end_row * geometry.scan_lines_per_row;
      if (end_y > adapter->frame_buffer_height) {
        end_y = adapter->frame_buffer_height;
      }
      VideoRenderDirtyRegion(
          video, range.start_column, range.end_column, first_y, end_y);
      row = end_row;
    }
  }

  static const VideoDirtyState kEmptyDirtyState = {0};
  video->dirty_state = kEmptyDirtyState;
}
