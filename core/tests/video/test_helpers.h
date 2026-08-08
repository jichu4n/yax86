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

inline uint8_t MockReadVRAMByte(VideoState* video, uint32_t address) {
  if (address < sizeof(mock_vram)) {
    return mock_vram[address];
  }
  return 0xFF;
}

inline void MockWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (address < sizeof(mock_vram)) {
    mock_vram[address] = value;
  }
}

// Mock frame buffer, sized for the larger of the two adapters. Rendering writes
// into it the same way a real display would, so a test can simply ask what
// color a pixel ended up.
enum {
  kMaxFrameBufferWidth = 720,
  kMaxFrameBufferHeight = 350,
};
inline RGB mock_frame_buffer[kMaxFrameBufferHeight][kMaxFrameBufferWidth];
// Number of write_pixel calls since the frame buffer was last cleared.
inline int mock_pixel_write_count;

inline void MockWritePixel(VideoState* video, Position position, RGB rgb) {
  ++mock_pixel_write_count;
  if (position.x < kMaxFrameBufferWidth && position.y < kMaxFrameBufferHeight) {
    mock_frame_buffer[position.y][position.x] = rgb;
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

    config_ = kDefaultVideoConfig;
    config_.adapter = adapter;
    config_.read_vram_byte = MockReadVRAMByte;
    config_.write_vram_byte = MockWriteVRAMByte;
    config_.write_pixel = MockWritePixel;

    VideoInit(&video_, &config_);
  }

  void ClearFrameBuffer() {
    static const RGB kNoPixel = {.r = 0xDE, .g = 0xAD, .b = 0xBE};
    for (int y = 0; y < kMaxFrameBufferHeight; ++y) {
      for (int x = 0; x < kMaxFrameBufferWidth; ++x) {
        mock_frame_buffer[y][x] = kNoPixel;
      }
    }
    mock_pixel_write_count = 0;
  }

  // Render a frame into a freshly cleared frame buffer.
  void Render() {
    ClearFrameBuffer();
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
    mock_vram[cell * 2] = character;
    mock_vram[cell * 2 + 1] = attribute;
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

  VideoConfig config_ = {0};
  VideoState video_ = {0};
};

}  // namespace video_test

#endif  // YAX86_TESTS_VIDEO_TEST_HELPERS_H
