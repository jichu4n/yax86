// Tests for behavior that is common to both video adapters.
#include "video.h"

#include <gtest/gtest.h>

#include "test_helpers.h"

namespace {

using namespace video_test;

TEST(VideoDirtyStateTest, RangeStorageIsZeroInitializedAndCompact) {
  YVideoDirtyState dirty = {0};
  EXPECT_EQ(sizeof(dirty.ranges), static_cast<size_t>(kVideoDirtyRowCount * 2));
  EXPECT_EQ(sizeof(dirty), sizeof(dirty.ranges) + sizeof(YVideoDirtyStatus));
  // The tracker competes with the emulated guest for MCU SRAM, so its size is
  // part of the design rather than an implementation detail. 50 rows cover the
  // tallest graphics mode; the 25 rows of a text mode fit within that.
  EXPECT_EQ(sizeof(dirty), static_cast<size_t>(104));
  // Clearing the tracker is a zero initialization, which relies on the clean
  // state and the empty range both being zero.
  EXPECT_EQ(dirty.status, kVideoClean);
  for (const YVideoDirtyRange& range : dirty.ranges) {
    EXPECT_EQ(range.start_column, range.end_column);
  }
}

class VideoTimingTest : public VideoTestBase,
                        public ::testing::WithParamInterface<YVideoAdapter> {
 protected:
  void SetUp() override { Init(GetParam()); }

  uint16_t StatusPort() const {
    return GetParam() == kVideoAdapterCGA ? uint16_t{kCGAPortStatus}
                                          : uint16_t{kMDAPortStatus};
  }

  uint8_t Status() { return YVideoReadPort(&video_, StatusPort()); }

  // Advance the beam by whole scan lines. YVideoTick takes a single
  // instruction's worth of cycles, so a whole frame has to be stepped a line
  // at a time.
  void AdvanceScanLines(uint16_t scan_lines) {
    const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);
    for (uint16_t i = 0; i < scan_lines; ++i) {
      YVideoTick(&video_, adapter->cycles_per_scan_line);
    }
  }
};

TEST_P(VideoTimingTest, StatusStartsInActiveDisplay) {
  EXPECT_EQ(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(Status() & kVideoStatusVerticalRetrace, 0);
  // No light pen is emulated, so its switch always reads as off.
  EXPECT_NE(Status() & kVideoStatusLightPenSwitchOff, 0);
  EXPECT_EQ(Status() & kVideoStatusLightPenTrigger, 0);
}

TEST_P(VideoTimingTest, HorizontalRetraceWithinAScanLine) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);

  // Partway through the active part of a scan line the display is still on.
  YVideoTick(&video_, adapter->display_cycles_per_scan_line - 1);
  EXPECT_EQ(Status() & kVideoStatusDisplayDisabled, 0);

  // Past the end of it, the display is disabled but there is no vertical
  // retrace yet.
  YVideoTick(&video_, 1);
  EXPECT_NE(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(Status() & kVideoStatusVerticalRetrace, 0);

  // The next scan line starts with the display back on.
  YVideoTick(
      &video_,
      adapter->cycles_per_scan_line - adapter->display_cycles_per_scan_line);
  EXPECT_EQ(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(video_.scan_line, 1);
}

TEST_P(VideoTimingTest, VerticalRetraceAtTheEndOfAFrame) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);

  // Run to the last displayed scan line.
  AdvanceScanLines(adapter->displayed_scan_lines - 1);
  EXPECT_EQ(Status() & kVideoStatusVerticalRetrace, 0);

  // One scan line later the vertical retrace begins, and stays set for the
  // rest of the frame.
  AdvanceScanLines(1);
  EXPECT_NE(Status() & kVideoStatusVerticalRetrace, 0);
  EXPECT_NE(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(video_.frames, 0u);

  AdvanceScanLines(
      adapter->scan_lines_per_frame - adapter->displayed_scan_lines - 1);
  EXPECT_NE(Status() & kVideoStatusVerticalRetrace, 0);

  // And clears at the start of the next frame.
  AdvanceScanLines(1);
  EXPECT_EQ(Status() & kVideoStatusVerticalRetrace, 0);
  EXPECT_EQ(video_.scan_line, 0);
  EXPECT_EQ(video_.frames, 1u);
}

TEST_P(VideoTimingTest, TickAccumulatesPartialScanLines) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);
  // A cycle count that does not divide the scan line period still advances the
  // beam at the right average rate.
  const uint16_t kCyclesPerTick = 17;
  uint32_t total_cycles = 0;
  for (int i = 0; i < 100; ++i) {
    YVideoTick(&video_, kCyclesPerTick);
    total_cycles += kCyclesPerTick;
  }
  EXPECT_EQ(video_.scan_line, total_cycles / adapter->cycles_per_scan_line);
  EXPECT_EQ(
      video_.scan_line_cycles, total_cycles % adapter->cycles_per_scan_line);
}

TEST_P(VideoTimingTest, StatusHighBitsStayClear) {
  // Bit 7 in particular: the BIOS probes it on the MDA status port to detect a
  // Hercules adapter, which this is not.
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);
  for (uint16_t i = 0; i < adapter->scan_lines_per_frame; ++i) {
    EXPECT_EQ(Status() & 0xF0, 0);
    YVideoTick(&video_, adapter->cycles_per_scan_line);
  }
}

TEST_P(VideoTimingTest, UnselectedRegisterReadsAsUnmapped) {
  uint16_t index_port = GetParam() == kVideoAdapterCGA
                            ? uint16_t{kCGAPortRegisterIndex}
                            : uint16_t{kMDAPortRegisterIndex};
  uint16_t data_port = GetParam() == kVideoAdapterCGA
                           ? uint16_t{kCGAPortRegisterData}
                           : uint16_t{kMDAPortRegisterData};
  // The 6845 has 18 registers, and the index masks to five bits, so indices 18
  // to 31 select nothing.
  YVideoWritePort(&video_, index_port, kNumCRTCRegisters);
  EXPECT_EQ(YVideoReadPort(&video_, data_port), 0xFF);
  YVideoWritePort(&video_, data_port, 0x55);
  EXPECT_EQ(YVideoReadPort(&video_, data_port), 0xFF);
}

TEST_P(VideoTimingTest, RenderIsSafeWithoutCallbacks) {
  // A host that has not installed its callbacks yet must not crash the
  // renderer. The platform initializes the video module before the host has a
  // chance to install them.
  YVideoConfig bare_config = YVideoDefaultConfig;
  bare_config.adapter = GetParam();
  YVideoState bare_video = {0};
  YVideoInit(&bare_video);
  YVideoRender(&bare_video);
  YVideoTick(&bare_video, 1000);
  EXPECT_EQ(YVideoReadVRAM(&bare_video, 0), 0xFF);
}

INSTANTIATE_TEST_SUITE_P(
    Adapters, VideoTimingTest,
    ::testing::Values(kVideoAdapterMDA, kVideoAdapterCGA),
    [](const ::testing::TestParamInfo<YVideoAdapter>& info) {
      return info.param == kVideoAdapterCGA ? "CGA" : "MDA";
    });

}  // namespace
