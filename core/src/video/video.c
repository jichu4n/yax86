#ifndef Y_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // Y_IMPLEMENTATION

#define Y_VIDEO_LOG(level, ...) \
  Y_LOG(video->config.logger, &YVideoLogModule, level, __VA_ARGS__)

// Power-on 6845 register values for the IBM Monochrome Display.
Y_FILE_PRIVATE const uint8_t
    YVideoDefaultMDARegisters[Y_VIDEO_NUM_CRTC_REGISTERS] = {
        0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
        0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Power-on 6845 register values for the CGA in 80x25 text mode, matching the
// values GLaBIOS programs for that mode.
Y_FILE_PRIVATE const uint8_t
    YVideoDefaultCGARegisters[Y_VIDEO_NUM_CRTC_REGISTERS] = {
        0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02,
        0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // Value of bits 6-5 of the cursor start register that disables the cursor.
  Y_VIDEO_CRTC_CURSOR_DISABLED = 0x20,
  // Mask of bits 6-5 of the cursor start register.
  Y_VIDEO_CRTC_CURSOR_MODE_MASK = 0x60,
  // Mask of the scan line select bits in the cursor start and end registers.
  Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK = 0x1F,

  // The 6845 latches only the low five bits of the register index.
  Y_VIDEO_CRTC_REGISTER_INDEX_MASK = 0x1F,

  // Value returned for reads that decode to nothing.
  Y_VIDEO_UNMAPPED_PORT_VALUE = 0xFF,

  Y_VIDEO_TEXT_COLUMNS40 = 40,
  Y_VIDEO_TEXT_COLUMNS80 = 80,
  Y_VIDEO_TEXT40_COLUMN_SCALE = 2,
  Y_VIDEO_TEXT80_COLUMN_SCALE = 1,
};

Y_FILE_PRIVATE void YVideoInvalidateVRAMAddress(
    YVideoState* video, uint32_t address);
Y_FILE_PRIVATE void YVideoInvalidateCursor(YVideoState* video);
Y_FILE_PRIVATE void YVideoInvalidateBlinkingText(YVideoState* video);
Y_FILE_PRIVATE void YVideoInvalidateAll(YVideoState* video);

Y_PUBLIC const YVideoAdapterMetadata* YVideoGetAdapterMetadata(
    const YVideoState* video) {
  return &YVideoAdapterMetadataTable[video->adapter];
}

// ============================================================================
// Video RAM
// ============================================================================

Y_MODULE_PRIVATE uint8_t
YVideoReadVRAMByte(YVideoState* video, uint32_t address) {
  if (!video->config.vram) {
    return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
  // VRAM is aliased throughout the adapter's window, so an address past the end
  // wraps around rather than reading nothing. The renderer relies on this when
  // the 6845 start address pushes it past the end of VRAM.
  //
  // Every adapter's VRAM size is a power of two, so the wrap is a mask rather
  // than a remainder. The size is only known at run time, so a remainder would
  // compile to a hardware divide in the middle of the render loop - and the
  // Cortex-M0+ this targets has no divide instruction at all.
  uint32_t vram_size = YVideoGetAdapterMetadata(video)->vram_size;
  return video->config.vram[address & (vram_size - 1)];
}

Y_MODULE_PRIVATE void YVideoWriteVRAMByte(
    YVideoState* video, uint32_t address, uint8_t value) {
  if (!video->config.vram) {
    return;
  }
  uint32_t vram_size = YVideoGetAdapterMetadata(video)->vram_size;
  address &= vram_size - 1;
  if (video->config.vram[address] == value) {
    return;
  }
  video->config.vram[address] = value;
  if (video->dirty_state.status == Y_VIDEO_FULL_REDRAW) {
    return;
  }
  YVideoInvalidateVRAMAddress(video, address);
}

Y_PUBLIC uint8_t YVideoReadVRAM(YVideoState* video, uint32_t address) {
  if (address >= YVideoGetAdapterMetadata(video)->vram_size) {
    return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
  return YVideoReadVRAMByte(video, address);
}

Y_PUBLIC void YVideoWriteVRAM(
    YVideoState* video, uint32_t address, uint8_t value) {
  if (address >= YVideoGetAdapterMetadata(video)->vram_size) {
    return;
  }
  YVideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

Y_PUBLIC void YVideoInit(YVideoState* video) {
  video->adapter = video->config.adapter == Y_VIDEO_ADAPTER_CGA
                       ? Y_VIDEO_ADAPTER_CGA
                       : Y_VIDEO_ADAPTER_MDA;

  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  const uint8_t* default_registers = video->adapter == Y_VIDEO_ADAPTER_CGA
                                         ? YVideoDefaultCGARegisters
                                         : YVideoDefaultMDARegisters;
  for (uint8_t i = 0; i < Y_VIDEO_NUM_CRTC_REGISTERS; ++i) {
    video->registers[i] = default_registers[i];
  }
  video->control_register = adapter->default_control_register;
  YVideoInvalidateAll(video);

  for (uint32_t i = 0; i < adapter->vram_size; i += 2) {
    YVideoWriteVRAMByte(video, i, ' ');
    YVideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// Video mode
// ============================================================================

Y_PUBLIC Y_HOT YVideoMode YVideoGetMode(const YVideoState* video) {
  if (video->adapter == Y_VIDEO_ADAPTER_MDA) {
    // The MDA has only one mode.
    return Y_VIDEO_MODE_MDA_TEXT_80X25;
  }
  // Derive the CGA mode from the mode control register, matching the values
  // the BIOS writes for each of its INT 10h modes.
  //
  // The bits are tested in the order a real card resolves them: the high
  // resolution bit wins over the graphics bits, and the graphics bit over the
  // high resolution graphics bit. The BIOS never writes a combination where
  // the order matters, but software that ORs a bit into a saved mode byte can,
  // and this is the order 86Box's renderer decodes them in.
  if (video->control_register & Y_VIDEO_CONTROL_HIGH_RESOLUTION) {
    return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
               ? Y_VIDEO_MODE_CGA_TEXT_80X25_MONO
               : Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR;
  }
  if (!(video->control_register & Y_VIDEO_CONTROL_GRAPHICS)) {
    return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
               ? Y_VIDEO_MODE_CGA_TEXT_40X25_MONO
               : Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR;
  }
  if (video->control_register & Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS) {
    return Y_VIDEO_MODE_CGA_GRAPHICS_640X200;
  }
  return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
             ? Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT
             : Y_VIDEO_MODE_CGA_GRAPHICS_320X200;
}

Y_PUBLIC const YVideoModeMetadata* YVideoGetModeMetadata(
    const YVideoState* video) {
  return &YVideoModeMetadataTable[YVideoGetMode(video)];
}

// ============================================================================
// 6845 CRT controller
// ============================================================================

Y_MODULE_PRIVATE uint16_t YVideoGetStartAddress(const YVideoState* video) {
  return (uint16_t)(video->registers[Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H]
                    << 8) |
         video->registers[Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L];
}

Y_MODULE_PRIVATE uint16_t YVideoGetCursorAddress(const YVideoState* video) {
  return (uint16_t)(video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_H] << 8) |
         video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_L];
}

Y_MODULE_PRIVATE bool YVideoIsCursorEnabled(const YVideoState* video) {
  return (video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_START] &
          Y_VIDEO_CRTC_CURSOR_MODE_MASK) != Y_VIDEO_CRTC_CURSOR_DISABLED;
}

Y_MODULE_PRIVATE uint8_t
YVideoGetCursorStartScanLine(const YVideoState* video) {
  return video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_START] &
         Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK;
}

Y_MODULE_PRIVATE uint8_t YVideoGetCursorEndScanLine(const YVideoState* video) {
  return video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_END] &
         Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK;
}

Y_MODULE_PRIVATE bool YVideoIsCursorBlinkOn(const YVideoState* video) {
  return (video->frames / Y_VIDEO_FRAMES_PER_CURSOR_BLINK_PHASE) % 2 == 0;
}

Y_MODULE_PRIVATE bool YVideoIsTextBlinkOn(const YVideoState* video) {
  return (video->frames / Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE) % 2 == 0;
}

Y_MODULE_PRIVATE bool YVideoGetVisibleCursorOffset(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset) {
  if (!YVideoIsCursorEnabled(video) || !YVideoIsCursorBlinkOn(video)) {
    return false;
  }
  *cursor_offset = (YVideoGetCursorAddress(video) - start_address) &
                   (metadata->vram_size / 2 - 1);
  return *cursor_offset < (uint16_t)metadata->columns * metadata->rows;
}

// ============================================================================
// Dirty regions
// ============================================================================

Y_FILE_PRIVATE void YVideoDirtyRangeAdd(
    YVideoDirtyRange* range, uint8_t start_column, uint8_t end_column) {
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
typedef struct YVideoDirtyGeometry {
  // Number of dirty rows covering the display.
  uint8_t rows;
  // Physical scan lines per dirty row.
  uint8_t scan_lines_per_row;
} YVideoDirtyGeometry;

Y_FILE_PRIVATE YVideoDirtyGeometry
YVideoGetDirtyGeometry(const YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  YVideoDirtyGeometry geometry;
  if (metadata->type == Y_VIDEO_MODE_TEXT) {
    geometry.rows = metadata->rows;
    geometry.scan_lines_per_row = metadata->char_height;
  } else {
    geometry.rows =
        (uint8_t)((metadata->height + Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP - 1) /
                  Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP);
    geometry.scan_lines_per_row = Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP;
  }
  if (geometry.rows > Y_VIDEO_DIRTY_ROW_COUNT) {
    geometry.rows = Y_VIDEO_DIRTY_ROW_COUNT;
  }
  return geometry;
}

Y_MODULE_PRIVATE void YVideoInvalidateRows(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row) {
  if (start_column >= end_column || end_column > Y_VIDEO_DIRTY_COLUMNS ||
      first_row >= end_row || first_row >= Y_VIDEO_DIRTY_ROW_COUNT) {
    return;
  }
  if (end_row > Y_VIDEO_DIRTY_ROW_COUNT) {
    end_row = Y_VIDEO_DIRTY_ROW_COUNT;
  }

  for (uint8_t row = first_row; row < end_row; ++row) {
    YVideoDirtyRangeAdd(
        &video->dirty_state.ranges[row], start_column, end_column);
  }
  // A pending full redraw already covers these rows, and downgrading it would
  // drop the rest of the frame.
  if (video->dirty_state.status == Y_VIDEO_CLEAN) {
    video->dirty_state.status = Y_VIDEO_DIRTY;
  }
}

Y_FILE_PRIVATE void YVideoInvalidateAll(YVideoState* video) {
  video->dirty_state.status = Y_VIDEO_FULL_REDRAW;
}

Y_FILE_PRIVATE void YVideoInvalidateTextCell(
    YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t cell_offset) {
  if (cell_offset >= (uint16_t)metadata->columns * metadata->rows) {
    return;
  }
  uint8_t row;
  uint8_t col;
  uint8_t column_scale;
  if (metadata->columns == Y_VIDEO_TEXT_COLUMNS40) {
    row = (uint8_t)(cell_offset / Y_VIDEO_TEXT_COLUMNS40);
    col = (uint8_t)(cell_offset % Y_VIDEO_TEXT_COLUMNS40);
    column_scale = Y_VIDEO_TEXT40_COLUMN_SCALE;
  } else {
    row = (uint8_t)(cell_offset / Y_VIDEO_TEXT_COLUMNS80);
    col = (uint8_t)(cell_offset % Y_VIDEO_TEXT_COLUMNS80);
    column_scale = Y_VIDEO_TEXT80_COLUMN_SCALE;
  }
  // The character row is the dirty row, so a changed cell marks exactly the
  // scan lines it covers - no conversion to scan lines and back.
  YVideoInvalidateRows(
      video, col * column_scale, (col + 1) * column_scale, row,
      (uint8_t)(row + 1));
}

Y_FILE_PRIVATE void YVideoInvalidateVRAMAddress(
    YVideoState* video, uint32_t address) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type == Y_VIDEO_MODE_TEXT) {
    uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
    uint16_t character_address = (uint16_t)(address / 2);
    uint16_t cell_offset =
        (character_address - YVideoGetStartAddress(video)) & character_mask;
    YVideoInvalidateTextCell(video, metadata, cell_offset);
    return;
  }

  // Each half of CGA graphics VRAM holds either the even or odd scan lines.
  uint32_t half_mask = Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET - 1;
  uint32_t start = (uint32_t)YVideoGetStartAddress(video) * 2 & half_mask;
  uint32_t offset = ((address & half_mask) - start) & half_mask;
  uint32_t visible_bytes =
      (metadata->height / 2) * Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE;
  if (offset >= visible_bytes) {
    return;
  }
  uint16_t row_in_half =
      (uint16_t)(offset / Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE);
  uint8_t byte_column = offset % Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE;
  uint16_t y = row_in_half * 2 +
               (address >= Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET ? 1 : 0);
  // Graphics modes have no character rows, so neighboring scan lines share a
  // dirty row.
  uint8_t dirty_row = (uint8_t)(y / Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP);
  YVideoInvalidateRows(
      video, byte_column, byte_column + 1, dirty_row, (uint8_t)(dirty_row + 1));
}

Y_FILE_PRIVATE void YVideoInvalidateCursor(YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type != Y_VIDEO_MODE_TEXT) {
    return;
  }
  uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
  uint16_t cursor_offset =
      (YVideoGetCursorAddress(video) - YVideoGetStartAddress(video)) &
      character_mask;
  YVideoInvalidateTextCell(video, metadata, cursor_offset);
}

Y_FILE_PRIVATE void YVideoInvalidateBlinkingText(YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type != Y_VIDEO_MODE_TEXT ||
      !(video->control_register & Y_VIDEO_CONTROL_ENABLE_BLINK)) {
    return;
  }

  uint16_t start_address = YVideoGetStartAddress(video);
  uint8_t column_scale = metadata->columns == Y_VIDEO_TEXT_COLUMNS40
                             ? Y_VIDEO_TEXT40_COLUMN_SCALE
                             : Y_VIDEO_TEXT80_COLUMN_SCALE;
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    uint8_t start_column = Y_VIDEO_DIRTY_COLUMNS;
    uint8_t end_column = 0;
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t attr_address = ((uint32_t)start_address + cell_offset) * 2 + 1;
      if (YVideoReadVRAMByte(video, attr_address) & Y_VIDEO_ATTRIBUTE_BLINK) {
        uint8_t dirty_column = col * column_scale;
        if (dirty_column < start_column) {
          start_column = dirty_column;
        }
        end_column = (col + 1) * column_scale;
      }
    }
    if (end_column != 0) {
      YVideoInvalidateRows(
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
// incrementing frames, which YVideoIsCursorBlinkOn() and YVideoIsTextBlinkOn()
// use to derive their blink phases.
Y_PUBLIC Y_HOT void YVideoTick(YVideoState* video, uint32_t cycles) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  if (adapter->cycles_per_scan_line == 0 ||
      adapter->scan_lines_per_frame == 0) {
    // No adapter geometry to advance through.
    return;
  }

  bool cursor_was_on = YVideoIsCursorBlinkOn(video);
  bool text_was_on = YVideoIsTextBlinkOn(video);

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

  if (cursor_was_on != YVideoIsCursorBlinkOn(video) &&
      YVideoIsCursorEnabled(video)) {
    YVideoInvalidateCursor(video);
  }
  if (text_was_on != YVideoIsTextBlinkOn(video)) {
    YVideoInvalidateBlinkingText(video);
  }
}

// The value the status port reads back, computed from where the CRT beam
// currently is.
Y_FILE_PRIVATE uint8_t YVideoGetStatus(const YVideoState* video) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  bool in_vertical_retrace = video->scan_line >= adapter->displayed_scan_lines;
  bool in_horizontal_retrace =
      video->scan_line_cycles >= adapter->display_cycles_per_scan_line;

  // No light pen is emulated, so its switch always reads as off.
  uint8_t status = Y_VIDEO_STATUS_LIGHT_PEN_SWITCH_OFF;
  if (in_vertical_retrace || in_horizontal_retrace) {
    status |= Y_VIDEO_STATUS_DISPLAY_DISABLED;
  }
  if (in_vertical_retrace) {
    status |= Y_VIDEO_STATUS_VERTICAL_RETRACE;
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
typedef enum YVideoPortFunction {
  // Port is not decoded by the adapter.
  Y_VIDEO_PORT_NONE = 0,
  // 6845 register index.
  Y_VIDEO_PORT_REGISTER_INDEX,
  // 6845 register data.
  Y_VIDEO_PORT_REGISTER_DATA,
  // Mode control register.
  Y_VIDEO_PORT_CONTROL,
  // CGA color select register.
  Y_VIDEO_PORT_COLOR_SELECT,
  // Status register.
  Y_VIDEO_PORT_STATUS,
} YVideoPortFunction;

// Decode an I/O port within the adapter's range.
Y_FILE_PRIVATE YVideoPortFunction
YVideoDecodePort(const YVideoState* video, uint16_t port) {
  if (video->adapter == Y_VIDEO_ADAPTER_MDA) {
    switch (port) {
      case Y_VIDEO_MDA_PORT_REGISTER_INDEX:
        return Y_VIDEO_PORT_REGISTER_INDEX;
      case Y_VIDEO_MDA_PORT_REGISTER_DATA:
        return Y_VIDEO_PORT_REGISTER_DATA;
      case Y_VIDEO_MDA_PORT_CONTROL:
        return Y_VIDEO_PORT_CONTROL;
      case Y_VIDEO_MDA_PORT_STATUS:
        return Y_VIDEO_PORT_STATUS;
      default:
        return Y_VIDEO_PORT_NONE;
    }
  }

  // The CGA only decodes the low four address bits, so the 6845 index and data
  // registers are aliased across 3D0 to 3D7.
  uint16_t offset = port - Y_VIDEO_CGA_PORT_START;
  if (offset < 8) {
    return offset & 1 ? Y_VIDEO_PORT_REGISTER_DATA
                      : Y_VIDEO_PORT_REGISTER_INDEX;
  }
  switch (port) {
    case Y_VIDEO_CGA_PORT_CONTROL:
      return Y_VIDEO_PORT_CONTROL;
    case Y_VIDEO_CGA_PORT_COLOR_SELECT:
      return Y_VIDEO_PORT_COLOR_SELECT;
    case Y_VIDEO_CGA_PORT_STATUS:
      return Y_VIDEO_PORT_STATUS;
    default:
      // The light pen strobe ports respond but do nothing, as no light pen is
      // emulated.
      return Y_VIDEO_PORT_NONE;
  }
}

Y_PUBLIC uint8_t YVideoReadPort(YVideoState* video, uint16_t port) {
  switch (YVideoDecodePort(video, port)) {
    case Y_VIDEO_PORT_REGISTER_INDEX:
      return video->selected_register;
    case Y_VIDEO_PORT_REGISTER_DATA:
      if (video->selected_register < Y_VIDEO_NUM_CRTC_REGISTERS) {
        return video->registers[video->selected_register];
      }
      return Y_VIDEO_UNMAPPED_PORT_VALUE;
    case Y_VIDEO_PORT_CONTROL:
      return video->control_register;
    case Y_VIDEO_PORT_COLOR_SELECT:
      return video->color_select_register;
    case Y_VIDEO_PORT_STATUS:
      return YVideoGetStatus(video);
    default:
      return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
}

Y_PUBLIC void YVideoWritePort(
    YVideoState* video, uint16_t port, uint8_t value) {
  switch (YVideoDecodePort(video, port)) {
    case Y_VIDEO_PORT_REGISTER_INDEX:
      video->selected_register = value & Y_VIDEO_CRTC_REGISTER_INDEX_MASK;
      break;
    case Y_VIDEO_PORT_REGISTER_DATA:
      if (video->selected_register < Y_VIDEO_NUM_CRTC_REGISTERS) {
        uint8_t* selected = &video->registers[video->selected_register];
        if (*selected == value) {
          break;
        }
        if (video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_H ||
            video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_L) {
          YVideoInvalidateCursor(video);
          *selected = value;
          YVideoInvalidateCursor(video);
        } else {
          *selected = value;
          if (video->selected_register ==
                  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H ||
              video->selected_register ==
                  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L) {
            YVideoInvalidateAll(video);
          } else if (
              video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_START ||
              video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_END) {
            YVideoInvalidateCursor(video);
          }
        }
      }
      break;
    case Y_VIDEO_PORT_CONTROL:
      if (video->control_register != value) {
        video->control_register = value;
        YVideoInvalidateAll(video);
      }
      break;
    case Y_VIDEO_PORT_COLOR_SELECT:
      if (video->color_select_register != value) {
        video->color_select_register = value;
        YVideoInvalidateAll(video);
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

Y_FILE_PRIVATE void YVideoRenderBlankRegion(
    YVideoState* video, YVideoPixelRun* run, YVideoRegion region) {
  YVideoRGB blank = video->adapter == Y_VIDEO_ADAPTER_CGA
                        ? video->config.cga_palette[0]
                        : video->config.background;
  uint16_t end_x = region.origin.x + region.width;
  uint16_t end_y = region.origin.y + region.height;
  for (uint16_t y = region.origin.y; y < end_y; ++y) {
    YVideoPixelRunBegin(run, region.origin.x, y);
    for (uint16_t x = region.origin.x; x < end_x; ++x) {
      YVideoPixelRunPush(run, blank);
    }
  }
}

Y_FILE_PRIVATE void YVideoRenderDirtyRegion(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  // Every mode divides the frame buffer into Y_VIDEO_DIRTY_COLUMNS equal
  // columns, so a dirty column is 9 pixels wide on the MDA and 8 on the CGA.
  uint16_t column_width = YVideoGetAdapterMetadata(video)->frame_buffer_width /
                          Y_VIDEO_DIRTY_COLUMNS;
  YVideoRegion region = {
      .origin = {.x = start_column * column_width, .y = first_y},
      .width = (end_column - start_column) * column_width,
      .height = end_y - first_y,
  };
  if (video->config.begin_render_region) {
    video->config.begin_render_region(video, region);
  }

  video->num_pixels_emitted_for_region = 0;
  // Only these two members need a value on the way in: origin is set whenever
  // a span starts, and a pixel is stored before the count that exposes it to
  // the flush. Initializing the whole struct would clear the 96-byte batch on
  // every region, which a graphics frame can split into fifty of them.
  YVideoPixelRun run;
  run.video = video;
  run.count = 0;

  if (!(video->control_register & Y_VIDEO_CONTROL_VIDEO_ENABLE)) {
    YVideoRenderBlankRegion(video, &run, region);
  } else if (video->adapter == Y_VIDEO_ADAPTER_CGA) {
    YVideoCGARenderRegion(
        video, &run, start_column, end_column, first_y, end_y);
  } else {
    YVideoMDARenderRegion(
        video, &run, start_column, end_column, first_y, end_y);
  }
  YVideoPixelRunFlush(&run);

  // A retained display addressed by transfer window advances its own write
  // pointer once per pixel, so it is the pixel count that positions them, not
  // the coordinates passed alongside. Emitting a different number than the
  // region declared leaves that pointer mid-window and displaces every pixel
  // after it, including those of later regions - so report the region that
  // caused it rather than let the corruption surface somewhere else.
  uint32_t declared_pixels = (uint32_t)region.width * region.height;
  if (video->num_pixels_emitted_for_region != declared_pixels) {
    Y_VIDEO_LOG(
        Y_LOG_LEVEL_ERROR,
        "render region %ux%u at (%u,%u) emitted %u pixels, expected %u",
        region.width, region.height, region.origin.x, region.origin.y,
        video->num_pixels_emitted_for_region, declared_pixels);
  }

  if (video->config.end_render_region) {
    video->config.end_render_region(video);
  }
}

Y_PUBLIC void YVideoRender(YVideoState* video) {
  if (!video->config.write_pixels ||
      video->dirty_state.status == Y_VIDEO_CLEAN) {
    return;
  }

  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  if (video->dirty_state.status == Y_VIDEO_FULL_REDRAW) {
    YVideoRenderDirtyRegion(
        video, 0, Y_VIDEO_DIRTY_COLUMNS, 0, adapter->frame_buffer_height);
  } else {
    YVideoDirtyGeometry geometry = YVideoGetDirtyGeometry(video);
    for (uint8_t row = 0; row < geometry.rows;) {
      YVideoDirtyRange range = video->dirty_state.ranges[row];
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
      YVideoRenderDirtyRegion(
          video, range.start_column, range.end_column, first_y, end_y);
      row = end_row;
    }
  }

  static const YVideoDirtyState kEmptyDirtyState = {0};
  video->dirty_state = kEmptyDirtyState;
}
