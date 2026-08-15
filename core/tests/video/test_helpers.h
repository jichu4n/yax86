#ifndef YAX86_TESTS_VIDEO_TEST_HELPERS_H
#define YAX86_TESTS_VIDEO_TEST_HELPERS_H

#include <gtest/gtest.h>

#include <cstdint>

#include "video.h"

// RGB is declared in the global namespace, so its comparison and printing
// helpers have to be too for GoogleTest to find them.
inline bool operator==(const RGB& a, const RGB& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Prints an RGB value on assertion failure rather than dumping its bytes.
inline void PrintTo(const RGB& rgb, std::ostream* os) {
  *os << "RGB(" << static_cast<int>(rgb.r) << ", " << static_cast<int>(rgb.g)
      << ", " << static_cast<int>(rgb.b) << ")";
}

namespace video_test {

// Mock VRAM, sized for the larger of the two adapters.
inline uint8_t mock_vram[kCGAVRAMSize];

// Mock frame buffer, sized for the larger of the two adapters. Rendering writes
// into it the same way a real display would, so a test can simply ask what
// color a pixel ended up.
enum {
  kMaxFrameBufferWidth = 720,
  kMaxFrameBufferHeight = 350,
};
inline RGB mock_frame_buffer[kMaxFrameBufferHeight][kMaxFrameBufferWidth];
// Render activity since the last call to Render().
inline int mock_pixel_write_count;
inline VideoRegion mock_regions[kVideoDirtyRowCount];
inline int mock_region_count;
inline int mock_region_end_count;
// The region currently open, and the pixels written into it so far. A real
// windowed display positions pixels by counting them, so a region that does
// not emit exactly the pixels it declared corrupts everything drawn after it.
inline VideoRegion mock_open_region;
inline int mock_open_region_pixels;
// Regions whose pixel count did not match the area they declared. Checked in
// the fixture's teardown, so every test enforces the invariant.
inline int mock_region_area_mismatches;
// Pixels written outside the region open at the time. A host that streams into
// a transfer window cannot see these - it places pixels by counting them - but
// one that addresses by coordinate, as the SDL runtime does, draws them in the
// wrong place.
inline int mock_pixels_outside_region;

inline void MockWritePixel(VideoState* video, Position position, RGB rgb) {
  ++mock_pixel_write_count;
  ++mock_open_region_pixels;
  if (position.x < mock_open_region.origin.x ||
      position.x >= mock_open_region.origin.x + mock_open_region.width ||
      position.y < mock_open_region.origin.y ||
      position.y >= mock_open_region.origin.y + mock_open_region.height) {
    ++mock_pixels_outside_region;
  }
  if (position.x < kMaxFrameBufferWidth && position.y < kMaxFrameBufferHeight) {
    mock_frame_buffer[position.y][position.x] = rgb;
  }
}

inline void MockBeginRenderRegion(VideoState* video, VideoRegion region) {
  if (mock_region_count < kVideoDirtyRowCount) {
    mock_regions[mock_region_count] = region;
  }
  ++mock_region_count;
  mock_open_region = region;
  mock_open_region_pixels = 0;
}

inline void MockEndRenderRegion(VideoState* video) {
  ++mock_region_end_count;
  const int declared_pixels =
      static_cast<int>(mock_open_region.width) * mock_open_region.height;
  if (mock_open_region_pixels != declared_pixels) {
    ++mock_region_area_mismatches;
  }
}

// Error-level log messages emitted since the last Init().
inline int mock_log_error_count;

inline void MockWriteLogLine(
    void* context, const LogModule* module, LogLevel level, uint64_t tick,
    const char* message, size_t length) {
  if (level == kLogLevelError) {
    ++mock_log_error_count;
  }
}

// Base fixture for the video tests. Subclasses call Init() with the adapter
// they exercise.
class VideoTestBase : public ::testing::Test {
 protected:
  void Init(VideoAdapter adapter) {
    for (size_t i = 0; i < sizeof(mock_vram); ++i) {
      mock_vram[i] = 0;
    }
    ClearFrameBuffer();
    mock_region_area_mismatches = 0;
    mock_pixels_outside_region = 0;
    mock_log_error_count = 0;

    logger_config_ = LoggerConfig{};
    logger_config_.write_line = MockWriteLogLine;
    logger_config_.enabled_modules = LogModuleMask(&kLogModuleVideo);
    logger_config_.min_level = kLogLevelError;
    LoggerInit(&logger_, &logger_config_);

    config_ = kDefaultVideoConfig;
    config_.adapter = adapter;
    config_.vram = mock_vram;
    config_.logger = &logger_;
    config_.write_pixel = MockWritePixel;
    config_.begin_render_region = MockBeginRenderRegion;
    config_.end_render_region = MockEndRenderRegion;

    VideoInit(&video_, &config_);
  }

  // Every rendered region must emit exactly the pixels it declared, or a
  // windowed host display would be corrupted from that region onward. A test
  // that deliberately provokes a mismatch consumes it by resetting the
  // counters before returning.
  void TearDown() override {
    EXPECT_EQ(mock_region_area_mismatches, 0)
        << "a rendered region emitted a different number of pixels than the "
           "region it declared to the host";
    EXPECT_EQ(mock_pixels_outside_region, 0)
        << "a rendered region wrote pixels outside the region it declared to "
           "the host";
    EXPECT_EQ(mock_log_error_count, 0);
  }

  void ClearFrameBuffer() {
    static const RGB kNoPixel = {.r = 0xDE, .g = 0xAD, .b = 0xBE};
    for (int y = 0; y < kMaxFrameBufferHeight; ++y) {
      for (int x = 0; x < kMaxFrameBufferWidth; ++x) {
        mock_frame_buffer[y][x] = kNoPixel;
      }
    }
    mock_pixel_write_count = 0;
    mock_region_count = 0;
    mock_region_end_count = 0;
  }

  void ResetRenderStats() {
    mock_pixel_write_count = 0;
    mock_region_count = 0;
    mock_region_end_count = 0;
  }

  // Render into the retained frame buffer, resetting only activity counters.
  void Render() {
    ResetRenderStats();
    VideoRender(&video_);
  }

  RGB Pixel(int x, int y) const { return mock_frame_buffer[y][x]; }

  // Number of pixels of the given color within a rectangle.
  int CountPixels(int x, int y, int width, int height, RGB rgb) const {
    int count = 0;
    for (int row = y; row < y + height; ++row) {
      for (int col = x; col < x + width; ++col) {
        if (mock_frame_buffer[row][col] == rgb) {
          ++count;
        }
      }
    }
    return count;
  }

  // Write a character and its attribute at a character cell offset.
  void WriteChar(uint32_t cell, uint8_t character, uint8_t attribute) {
    VideoWriteVRAM(&video_, cell * 2, character);
    VideoWriteVRAM(&video_, cell * 2 + 1, attribute);
  }

  // Set a 6845 register through the I/O ports.
  void WriteRegister(uint8_t index, uint8_t value) {
    uint16_t index_port = video_.adapter == kVideoAdapterCGA
                              ? uint16_t{kCGAPortRegisterIndex}
                              : uint16_t{kMDAPortRegisterIndex};
    uint16_t data_port = video_.adapter == kVideoAdapterCGA
                             ? uint16_t{kCGAPortRegisterData}
                             : uint16_t{kMDAPortRegisterData};
    VideoWritePort(&video_, index_port, index);
    VideoWritePort(&video_, data_port, value);
  }

  // Turn the text mode cursor off, so that it does not overdraw the character
  // cell a test is inspecting.
  void DisableCursor() {
    uint8_t selected_register = video_.selected_register;
    WriteRegister(kCRTCRegisterCursorStart, 0x20);
    video_.selected_register = selected_register;
  }

  // Advance the CRT beam by whole frames. VideoTick takes a single
  // instruction's worth of cycles, which is far less than a frame, so this
  // steps a scan line at a time.
  void AdvanceFrames(int frames) {
    const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
    for (int frame = 0; frame < frames; ++frame) {
      for (uint16_t i = 0; i < adapter->scan_lines_per_frame; ++i) {
        VideoTick(&video_, adapter->cycles_per_scan_line);
      }
    }
  }

  LoggerConfig logger_config_ = {};
  Logger logger_ = {};
  VideoConfig config_ = {0};
  VideoState video_ = {0};
};

}  // namespace video_test

#endif  // YAX86_TESTS_VIDEO_TEST_HELPERS_H
