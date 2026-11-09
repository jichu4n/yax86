// Tests for rendering from a copy of the video state, for a host that renders
// somewhere the machine is not running.
#include <gtest/gtest.h>

#include <vector>

#include "test_helpers.h"
#include "video.h"

namespace {

using namespace video_test;

enum {
  // The CGA's 8x8 cell, and the full block, which fills it.
  kCellWidth = 8,
  kCellHeight = 8,
  kSolidBlock = 0xDB,
  kSpace = 0x20,
  // Red on black, and white on black.
  kRedOnBlack = 0x04,
  kWhiteOnBlack = 0x0F,
  kColorRed = 4,
  kColorWhite = 15,

  // Cells on different rows, so that each is a region of its own.
  kFirstCell = 0,
  kSecondCell = 3 * 80 + 10,
  kLaterCell = 12 * 80 + 40,
};

class CopyForRenderTest : public VideoTestBase {
 protected:
  void SetUp() override {
    Init(Y_VIDEO_ADAPTER_CGA);
    DisableCursor();
  }

  // Put something on the screen worth comparing.
  void DrawSomething() {
    WriteChar(kFirstCell, kSolidBlock, kRedOnBlack);
    WriteChar(kSecondCell, kSolidBlock, kWhiteOnBlack);
  }

  bool CopyForRender() {
    return YVideoCopyForRender(&video_, &frame_, frame_vram_);
  }

  void RenderFrame() {
    ResetRenderStats();
    YVideoRender(&frame_);
  }

  YVideoRGB CGAColor(uint8_t index) const {
    return YVideoDefaultConfig.cga_palette[index];
  }

  // Pixels of the given color in a cell of the 80 column text screen.
  int CellPixels(uint32_t cell, YVideoRGB rgb) const {
    return CountPixels(
        (cell % 80) * kCellWidth, (cell / 80) * kCellHeight, kCellWidth,
        kCellHeight, rgb);
  }

  std::vector<YVideoRGB> FrameBuffer() const {
    return std::vector<YVideoRGB>(
        &mock_frame_buffer[0][0],
        &mock_frame_buffer[0][0] +
            kMaxFrameBufferWidth * kMaxFrameBufferHeight);
  }

  YVideoState frame_ = {};
  uint8_t frame_vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
};

// The copy draws the same pixels, in the same regions, as rendering the live
// state would have.
TEST_F(CopyForRenderTest, ACopyDrawsWhatTheLiveStateWould) {
  Render();
  DrawSomething();
  Render();
  const std::vector<YVideoRGB> live = FrameBuffer();
  const int live_pixels = mock_pixel_write_count;
  const int live_regions = mock_region_count;

  Init(Y_VIDEO_ADAPTER_CGA);
  DisableCursor();
  Render();
  DrawSomething();
  ASSERT_TRUE(CopyForRender());
  RenderFrame();

  EXPECT_EQ(mock_pixel_write_count, live_pixels);
  EXPECT_EQ(mock_region_count, live_regions);
  EXPECT_EQ(FrameBuffer(), live);
  EXPECT_EQ(CellPixels(kFirstCell, CGAColor(kColorRed)), 64);
  EXPECT_EQ(CellPixels(kSecondCell, CGAColor(kColorWhite)), 64);
}

// What changed went with the copy, so the live state has nothing left to draw.
TEST_F(CopyForRenderTest, TheLiveStateIsUpToDateAfterACopy) {
  DrawSomething();
  ASSERT_TRUE(CopyForRender());

  Render();

  EXPECT_EQ(mock_pixel_write_count, 0);
  EXPECT_EQ(video_.dirty_state.status, Y_VIDEO_CLEAN);
}

// A change made while the copy is being drawn is neither drawn from it nor
// lost: the next copy draws it, and only it.
TEST_F(CopyForRenderTest, AChangeAfterACopyIsDrawnFromTheNext) {
  Render();
  DrawSomething();
  ASSERT_TRUE(CopyForRender());
  WriteChar(kLaterCell, kSolidBlock, kWhiteOnBlack);

  RenderFrame();
  EXPECT_EQ(CellPixels(kLaterCell, CGAColor(kColorWhite)), 0);

  ASSERT_TRUE(CopyForRender());
  RenderFrame();
  EXPECT_EQ(CellPixels(kLaterCell, CGAColor(kColorWhite)), 64);
  EXPECT_EQ(mock_region_count, 1);
}

// The copy draws from its own video memory, so the machine writing to the live
// memory meanwhile does not reach it.
TEST_F(CopyForRenderTest, TheCopyDrawsFromItsOwnVideoMemory) {
  Render();
  DrawSomething();
  ASSERT_TRUE(CopyForRender());
  WriteChar(kFirstCell, kSpace, kWhiteOnBlack);

  RenderFrame();

  EXPECT_EQ(frame_.config.vram, frame_vram_);
  EXPECT_EQ(CellPixels(kFirstCell, CGAColor(kColorRed)), 64);
}

TEST_F(CopyForRenderTest, NothingChangedCopiesNothing) {
  Render();

  EXPECT_FALSE(CopyForRender());
  EXPECT_EQ(frame_.config.vram, nullptr);
}

}  // namespace
