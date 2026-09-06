#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

constexpr uint16_t kProgramOffset = 0x0100;
// Somewhere in conventional memory well clear of the program.
constexpr uint16_t kDataOffset = 0x0200;
// Segment whose base sits 16 bytes below the top of conventional memory, so
// that a word read at offset 0x000F straddles the top of the window without
// wrapping the offset.
constexpr uint16_t kSegmentBelowTopOfRAM = 0x0FFF;
// CGA video memory, which is mapped but is not conventional memory.
constexpr uint16_t kCGASegment = 0xB800;

enum : uint8_t {
  kOpHlt = 0xF4,
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
  // MOV AL, moffs8
  kOpMovAlMoffs8 = 0xA0,
  // MOV moffs8, AL
  kOpMovMoffs8Al = 0xA2,
  // MOV AX, moffs16
  kOpMovAxMoffs16 = 0xA1,
};

// Operands are read and written by indexing the host's memory directly when
// the platform hands the CPU a window over it. These pin down where the window
// stops - the addresses above it, and the debug features it may not hide.
class PlatformDirectDataWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);
    config_.context = this;
    config_.physical_memory = ram_;
    config_.vram = vram_;
    // Named rather than left to default, since kCGASegment below and the size
    // of vram_ both depend on which adapter is mapped.
    config_.video_adapter = kVideoAdapterCGA;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
    platform_.cpu.registers[kCS] = 0;
    platform_.cpu.registers[kIP] = kProgramOffset;
    platform_.cpu.registers[kDS] = 0;
    platform_.cpu.registers[kSS] = 0;
    platform_.cpu.registers[kSP] = 0xFFFE;
  }

  void Load(const std::vector<uint8_t>& code) {
    for (size_t i = 0; i < code.size(); ++i) {
      ram_[kProgramOffset + i] = code[i];
    }
  }

  PlatformRunStatus RunInstructions(int num_instructions) {
    PlatformRunStatus status = kPlatformRunning;
    for (int i = 0; i < num_instructions; ++i) {
      status = PlatformTick(&platform_);
      if (status != kPlatformRunning) {
        return status;
      }
    }
    return status;
  }

  uint8_t al() const { return platform_.cpu.registers[kAX] & 0xFF; }
  uint16_t ax() const { return platform_.cpu.registers[kAX]; }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
};

TEST_F(PlatformDirectDataWindowTest, OpensAWindowOverConventionalMemory) {
  EXPECT_EQ(platform_.cpu.direct_data_window.data, ram_);
  EXPECT_EQ(platform_.cpu.direct_data_window.end, sizeof(ram_));
}

TEST_F(PlatformDirectDataWindowTest, OperandReadsAndWritesGoThroughTheWindow) {
  ram_[kDataOffset] = 0x11;
  Load(
      {kOpMovAlImm8, 0x42,
       // MOV [kDataOffset], AL
       kOpMovMoffs8Al, kDataOffset & 0xFF, kDataOffset >> 8,
       // MOV AL, [kDataOffset + 1]
       kOpMovAlMoffs8, (kDataOffset + 1) & 0xFF, (kDataOffset + 1) >> 8,
       kOpHlt});
  ram_[kDataOffset + 1] = 0x5A;

  ASSERT_EQ(RunInstructions(3), kPlatformRunning);
  EXPECT_EQ(ram_[kDataOffset], 0x42);
  EXPECT_EQ(al(), 0x5A);
}

// The window is a pointer into the host's own storage rather than a copy of
// it, so a write the host makes itself - or one made by DMA - is visible to
// the next access with nothing to invalidate.
TEST_F(PlatformDirectDataWindowTest, HostWritesAreVisibleThroughTheWindow) {
  Load({kOpMovAlMoffs8, kDataOffset & 0xFF, kDataOffset >> 8, kOpHlt});
  ram_[kDataOffset] = 0x7E;

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(al(), 0x7E);
}

// Video memory is mapped, but its writes go to the adapter rather than to a
// buffer, so it is above the window and takes the ordinary path.
TEST_F(
    PlatformDirectDataWindowTest, AddressesAboveTheWindowTakeTheOrdinaryPath) {
  Load({kOpMovAlImm8, 0x41, kOpMovMoffs8Al, 0x00, 0x00, kOpHlt});
  platform_.cpu.registers[kDS] = kCGASegment;

  ASSERT_EQ(RunInstructions(2), kPlatformRunning);
  EXPECT_EQ(vram_[0], 0x41);
}

// A word whose low byte is the last byte of the window has its high byte
// outside it, so the two halves take different paths. Nothing above
// conventional memory is mapped here, so the high byte reads as open bus.
TEST_F(PlatformDirectDataWindowTest, WordStraddlingTheTopOfTheWindow) {
  Load({kOpMovAxMoffs16, 0x0F, 0x00, kOpHlt});
  platform_.cpu.registers[kDS] = kSegmentBelowTopOfRAM;
  ram_[sizeof(ram_) - 1] = 0x34;

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(ax(), 0xFF34);
}

// An access through the window is a load or a store, so the platform has to
// take the window away rather than let a watchpoint go unnoticed.
TEST_F(PlatformDirectDataWindowTest, WatchpointOnAnOperandWriteStillFires) {
  Load(
      {kOpMovAlImm8, 0x42, kOpMovMoffs8Al, kDataOffset & 0xFF, kDataOffset >> 8,
       kOpHlt});

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/false,
          /*on_write=*/true),
      0);
  EXPECT_EQ(platform_.cpu.direct_data_window.data, nullptr);
  EXPECT_EQ(platform_.cpu.direct_data_window.end, 0u);

  EXPECT_EQ(RunInstructions(3), kPlatformStopped);
  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopMemoryWatchpoint);
  EXPECT_EQ(stop_info->address, (uint32_t)kDataOffset);
  EXPECT_TRUE(stop_info->is_write);
}

TEST_F(PlatformDirectDataWindowTest, WatchpointOnAnOperandReadStillFires) {
  Load({kOpMovAlMoffs8, kDataOffset & 0xFF, kDataOffset >> 8, kOpHlt});

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/true,
          /*on_write=*/false),
      0);
  ASSERT_EQ(platform_.cpu.direct_data_window.data, nullptr);

  EXPECT_EQ(RunInstructions(2), kPlatformStopped);
  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopMemoryWatchpoint);
  EXPECT_EQ(stop_info->address, (uint32_t)kDataOffset);
  EXPECT_FALSE(stop_info->is_write);
}

TEST_F(PlatformDirectDataWindowTest, ClearingWatchpointsHandsTheWindowBack) {
  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/true,
          /*on_write=*/false),
      0);
  ASSERT_EQ(platform_.cpu.direct_data_window.data, nullptr);

  PlatformClearMemoryWatchpoints(&platform_);
  EXPECT_EQ(platform_.cpu.direct_data_window.data, ram_);
  EXPECT_EQ(platform_.cpu.direct_data_window.end, sizeof(ram_));
}

// Registering a region recomputes the window rather than discarding it, so a
// region that has nothing to do with address 0 leaves it in place.
TEST_F(
    PlatformDirectDataWindowTest, RegisteringAnUnrelatedRegionKeepsTheWindow) {
  static uint8_t region[4096] = {0};
  MemoryMapEntry entry = {0};
  entry.entry_type = 0x7F;
  entry.start = 0xD0000;
  entry.end = 0xD0000 + sizeof(region) - 1;
  entry.read_data = region;
  entry.write_data = region;
  ASSERT_TRUE(RegisterMemoryMapEntry(&platform_, &entry));

  EXPECT_EQ(platform_.cpu.direct_data_window.data, ram_);
  EXPECT_EQ(platform_.cpu.direct_data_window.end, sizeof(ram_));
}

}  // namespace
