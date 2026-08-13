// Tests for behavior that is common to both video adapters.
#include "video.h"

#include <gtest/gtest.h>

#include "test_helpers.h"

namespace {

using namespace video_test;

TEST(VideoDirtyStateTest, RangeStorageIsZeroInitializedAndCompact) {
  VideoDirtyState dirty = {0};
  EXPECT_EQ(
      sizeof(dirty.ranges), static_cast<size_t>(kVideoDirtyGroupCount * 2));
  EXPECT_EQ(sizeof(dirty), sizeof(dirty.ranges) + 2);
  for (const VideoDirtyRange& range : dirty.ranges) {
    EXPECT_EQ(range.first_column, range.end_column);
  }
}

class VideoTimingTest : public VideoTestBase,
                        public ::testing::WithParamInterface<VideoAdapter> {
 protected:
  void SetUp() override { Init(GetParam()); }

  uint16_t StatusPort() const {
    return GetParam() == kVideoAdapterCGA ? uint16_t{kCGAPortStatus}
                                          : uint16_t{kMDAPortStatus};
  }

  uint8_t Status() { return VideoReadPort(&video_, StatusPort()); }

  // Advance the beam by whole scan lines. VideoTick takes a single
  // instruction's worth of cycles, so a whole frame has to be stepped a line
  // at a time.
  void AdvanceScanLines(uint16_t scan_lines) {
    const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
    for (uint16_t i = 0; i < scan_lines; ++i) {
      VideoTick(&video_, adapter->cycles_per_scan_line);
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
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);

  // Partway through the active part of a scan line the display is still on.
  VideoTick(&video_, adapter->display_cycles_per_scan_line - 1);
  EXPECT_EQ(Status() & kVideoStatusDisplayDisabled, 0);

  // Past the end of it, the display is disabled but there is no vertical
  // retrace yet.
  VideoTick(&video_, 1);
  EXPECT_NE(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(Status() & kVideoStatusVerticalRetrace, 0);

  // The next scan line starts with the display back on.
  VideoTick(
      &video_,
      adapter->cycles_per_scan_line - adapter->display_cycles_per_scan_line);
  EXPECT_EQ(Status() & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(video_.scan_line, 1);
}

TEST_P(VideoTimingTest, VerticalRetraceAtTheEndOfAFrame) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);

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
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
  // A cycle count that does not divide the scan line period still advances the
  // beam at the right average rate.
  const uint16_t kCyclesPerTick = 17;
  uint32_t total_cycles = 0;
  for (int i = 0; i < 100; ++i) {
    VideoTick(&video_, kCyclesPerTick);
    total_cycles += kCyclesPerTick;
  }
  EXPECT_EQ(video_.scan_line, total_cycles / adapter->cycles_per_scan_line);
  EXPECT_EQ(
      video_.scan_line_cycles, total_cycles % adapter->cycles_per_scan_line);
}

TEST_P(VideoTimingTest, StatusHighBitsStayClear) {
  // Bit 7 in particular: the BIOS probes it on the MDA status port to detect a
  // Hercules adapter, which this is not.
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
  for (uint16_t i = 0; i < adapter->scan_lines_per_frame; ++i) {
    EXPECT_EQ(Status() & 0xF0, 0);
    VideoTick(&video_, adapter->cycles_per_scan_line);
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
  VideoWritePort(&video_, index_port, kNumCRTCRegisters);
  EXPECT_EQ(VideoReadPort(&video_, data_port), 0xFF);
  VideoWritePort(&video_, data_port, 0x55);
  EXPECT_EQ(VideoReadPort(&video_, data_port), 0xFF);
}

TEST_P(VideoTimingTest, RenderIsSafeWithoutCallbacks) {
  // A host that has not installed its callbacks yet must not crash the
  // renderer. The platform initializes the video module before the host has a
  // chance to install them.
  VideoConfig bare_config = kDefaultVideoConfig;
  bare_config.adapter = GetParam();
  VideoState bare_video = {0};
  VideoInit(&bare_video, &bare_config);
  VideoRender(&bare_video);
  VideoTick(&bare_video, 1000);
  EXPECT_EQ(VideoReadVRAM(&bare_video, 0), 0xFF);
}

INSTANTIATE_TEST_SUITE_P(
    Adapters, VideoTimingTest,
    ::testing::Values(kVideoAdapterMDA, kVideoAdapterCGA),
    [](const ::testing::TestParamInfo<VideoAdapter>& info) {
      return info.param == kVideoAdapterCGA ? "CGA" : "MDA";
    });

}  // namespace
