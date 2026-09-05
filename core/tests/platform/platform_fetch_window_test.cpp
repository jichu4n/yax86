#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

constexpr uint16_t kProgramOffset = 0x0100;

enum : uint8_t {
  kOpNop = 0x90,
  kOpHlt = 0xF4,
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
  // DEC CX
  kOpDecCx = 0x49,
  // JNZ rel8
  kOpJnzRel8 = 0x75,
  // JMP rel8
  kOpJmpRel8 = 0xEB,
};

// Instruction fetch reads through a direct window into the host's memory when
// the platform hands one out, and keeps that window across instructions. These
// pin down the cases where the window has to be given up - anything that
// changes what an address means, or runs off the end of it.
class PlatformFetchWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);
    config_.context = this;
    config_.physical_memory = ram_;
    config_.vram = vram_;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
    platform_.cpu.registers[kCS] = 0;
    platform_.cpu.registers[kIP] = kProgramOffset;
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

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
};

TEST_F(PlatformFetchWindowTest, OpensAWindowOverConventionalMemory) {
  Load({kOpNop, kOpNop, kOpHlt});
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_NE(platform_.cpu.instruction_fetch_window.data, nullptr);
}

// The window is a pointer into the host's own storage rather than a copy, so a
// write through the memory map is visible to the next fetch with no
// invalidation at all. This is the case that would break if it were a copy.
TEST_F(PlatformFetchWindowTest, SelfModifyingCodeIsVisibleThroughTheWindow) {
  Load({kOpNop, kOpNop, kOpNop, kOpHlt});
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  ASSERT_NE(platform_.cpu.instruction_fetch_window.data, nullptr);

  // Overwrite the two NOPs ahead of IP with MOV AL, 0x42.
  WriteMemoryByte(&platform_, kProgramOffset + 1, kOpMovAlImm8);
  WriteMemoryByte(&platform_, kProgramOffset + 2, 0x42);

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(al(), 0x42);
}

// A window bypasses the watchpoint check, so turning watchpoints on has to
// discard whichever one is already open. Without that, a watchpoint added
// after execution began would never fire on an instruction fetch.
TEST_F(PlatformFetchWindowTest, WatchpointAddedAfterAWindowIsOpenStillFires) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop, kOpHlt});
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  ASSERT_NE(platform_.cpu.instruction_fetch_window.data, nullptr);

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kProgramOffset + 2, kProgramOffset + 2,
          /*on_read=*/true, /*on_write=*/false),
      0);
  EXPECT_EQ(platform_.cpu.instruction_fetch_window.data, nullptr);

  EXPECT_EQ(RunInstructions(4), kPlatformStopped);
  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopMemoryWatchpoint);
  EXPECT_EQ(stop_info->address, (uint32_t)(kProgramOffset + 2));
  EXPECT_FALSE(stop_info->is_write);
}

TEST_F(PlatformFetchWindowTest, ClearingWatchpointsLetsAWindowOpenAgain) {
  Load({kOpNop, kOpNop, kOpNop, kOpHlt});
  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, 0xF000, 0xF000, /*on_read=*/true, /*on_write=*/false),
      0);
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  // No window is handed out at all while watchpoints are enabled.
  EXPECT_EQ(platform_.cpu.instruction_fetch_window.data, nullptr);

  PlatformClearMemoryWatchpoints(&platform_);
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_NE(platform_.cpu.instruction_fetch_window.data, nullptr);
}

// Registering a region moves addresses from one entry to another, so an open
// window may no longer describe what lives there.
TEST_F(PlatformFetchWindowTest, RegisteringAMemoryRegionDiscardsTheWindow) {
  Load({kOpNop, kOpNop, kOpHlt});
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  ASSERT_NE(platform_.cpu.instruction_fetch_window.data, nullptr);

  static uint8_t region[4096] = {0};
  MemoryMapEntry entry = {0};
  entry.entry_type = 0x7F;
  entry.start = 0xD0000;
  entry.end = 0xD0000 + sizeof(region) - 1;
  entry.read_data = region;
  entry.write_data = region;
  ASSERT_TRUE(RegisterMemoryMapEntry(&platform_, &entry));

  EXPECT_EQ(platform_.cpu.instruction_fetch_window.data, nullptr);
}

// IP wraps within the segment where a linear address does not, so the window
// has to stop at the wrap and let the ordinary path recompute the address.
TEST_F(
    PlatformFetchWindowTest, FetchWrappingIPMidInstructionReadsTheRightByte) {
  // MOV AL, 0x42 with the opcode at the last byte of the segment and its
  // immediate at the first.
  ram_[0xFFFF] = kOpMovAlImm8;
  ram_[0x0000] = 0x42;
  platform_.cpu.registers[kIP] = 0xFFFF;

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(al(), 0x42);
  EXPECT_EQ(platform_.cpu.registers[kIP], 0x0001);
}

// A window spans a whole memory region, so a jump backwards within that region
// lands inside the window already open. Nothing about the cursor having been
// advanced past those bytes stops them being read again - the window is held
// as a range of addresses, and the cursor is derived from it per instruction.
TEST_F(PlatformFetchWindowTest, BackwardJumpReusesTheSameWindow) {
  // DEC CX / JNZ -3 / HLT, run round the loop several times.
  Load({kOpDecCx, kOpJnzRel8, 0xFD, kOpHlt});
  platform_.cpu.registers[kCX] = 4;

  ASSERT_EQ(RunInstructions(2), kPlatformRunning);
  const uint8_t* window_after_first_pass =
      platform_.cpu.instruction_fetch_window.data;
  ASSERT_NE(window_after_first_pass, nullptr);

  // Three more passes, each of which jumps back over the two instructions it
  // just fetched.
  ASSERT_EQ(RunInstructions(6), kPlatformRunning);
  EXPECT_EQ(platform_.cpu.registers[kCX], 0u);
  EXPECT_EQ(
      platform_.cpu.instruction_fetch_window.data, window_after_first_pass);
}

// The window the platform hands out covers the whole region, not the tail of
// it from the address that was asked for, so a jump backwards to before the
// point fetching started still lands inside it.
TEST_F(PlatformFetchWindowTest, WindowCoversTheRegionBeforeTheFetchAddress) {
  // JMP -66, from 0x0100 back to 0x00C0.
  Load({kOpJmpRel8, 0xBE});
  ram_[0x00C0] = kOpNop;
  ram_[0x00C1] = kOpHlt;

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  const CPUInstructionFetchWindow window =
      platform_.cpu.instruction_fetch_window;
  ASSERT_NE(window.data, nullptr);
  // Conventional memory starts at zero, so the window reaches back past the
  // 0x0100 the first fetch asked for.
  EXPECT_EQ(window.start, 0u);
  EXPECT_LT(window.start, kProgramOffset);
  ASSERT_EQ(platform_.cpu.registers[kIP], 0x00C0);

  // The instruction at the jump target is fetched through the same window.
  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(platform_.cpu.instruction_fetch_window.data, window.data);
  EXPECT_EQ(platform_.cpu.instruction_fetch_window.start, window.start);
}

// Fetching past the end of a region falls back to the byte-at-a-time path,
// which resolves each address on its own.
TEST_F(PlatformFetchWindowTest, FetchRunningOffTheEndOfMemoryStillDecodes) {
  // The last byte of RAM is an opcode whose immediate lies in unmapped memory,
  // which reads as 0xFF. Nothing here should crash or read out of bounds.
  ram_[sizeof(ram_) - 1] = kOpMovAlImm8;
  platform_.cpu.registers[kCS] = 0x0FFF;
  platform_.cpu.registers[kIP] = 0x000F;  // Linear 0xFFFF, the last RAM byte.

  ASSERT_EQ(RunInstructions(1), kPlatformRunning);
  EXPECT_EQ(al(), 0xFF);
}

}  // namespace
