#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

constexpr uint16_t kProgramOffset = 0x0100;

enum : uint8_t {
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
};

// The CPU keeps decoded instructions and reuses them, which is only safe while
// everything that changes what is at an address says so. The platform is where
// most of those changes happen - DMA writes, a region being mapped, a
// watchpoint being enabled - so this is where they are pinned down.
class PlatformDecodeCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);
    config_.context = this;
    config_.physical_memory = ram_;
    config_.vram = vram_;
    config_.video_adapter = kVideoAdapterCGA;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
    platform_.cpu.registers[kCS] = 0;
    platform_.cpu.registers[kDS] = 0;
    platform_.cpu.registers[kSS] = 0;
    platform_.cpu.registers[kSP] = 0xFFFE;
  }

  // Writes instruction bytes straight into the host's buffer, which is what
  // makes a reused decode visible: nothing about this write reaches the CPU.
  void PokeBehindTheCPUsBack(
      uint16_t offset, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      ram_[offset + i] = bytes[i];
    }
  }

  void RunAt(uint16_t ip) {
    platform_.cpu.registers[kIP] = ip;
    ASSERT_EQ(PlatformTick(&platform_), kPlatformRunning);
  }

  uint8_t al() const { return platform_.cpu.registers[kAX] & 0xFF; }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
  uint8_t rom_[0x800] = {0};
};

TEST_F(PlatformDecodeCacheTest, ThePlatformHandsTheCPUItsDecodeCache) {
  EXPECT_EQ(platform_.cpu.decode_cache, platform_.cpu_decode_cache);
  EXPECT_EQ(platform_.cpu.decode_cache_index_mask, kDecodeCacheEntries - 1);
}

// What DMA takes. DOS loads itself over the boot sector this way, so an
// unreported write here is the difference between booting and not.
TEST_F(PlatformDecodeCacheTest, AWriteThroughTheMemoryMapDiscardsTheDecode) {
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x11});
  RunAt(kProgramOffset);
  ASSERT_EQ(al(), 0x11);

  WriteMemoryByte(&platform_, kProgramOffset + 1, 0x22);
  RunAt(kProgramOffset);
  EXPECT_EQ(al(), 0x22);
}

// Nothing was written here - what changed is which region owns the address, so
// the page generations have nothing to say and the whole cache goes.
TEST_F(PlatformDecodeCacheTest, RegisteringAMemoryRegionDiscardsEveryDecode) {
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x11});
  RunAt(kProgramOffset);
  ASSERT_EQ(al(), 0x11);

  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x22});
  MemoryMapEntry extra_rom = {};
  // Any type no module has claimed, mapped somewhere nothing else covers.
  extra_rom.entry_type = 0x7F;
  extra_rom.start = 0xD0000;
  extra_rom.end = 0xD07FF;
  extra_rom.read_data = rom_;
  ASSERT_TRUE(RegisterMemoryMapEntry(&platform_, &extra_rom));

  RunAt(kProgramOffset);
  EXPECT_EQ(al(), 0x22);
}

// A hit runs an instruction without reading its bytes, so it would hide a read
// watchpoint on the code being run. The platform takes the cache away for as
// long as any watchpoint is enabled, exactly as it does the two direct
// windows.
TEST_F(PlatformDecodeCacheTest, AWatchpointTakesTheCacheAway) {
  const int8_t index =
      PlatformAddMemoryWatchpoint(&platform_, 0x0200, 0x0200, true, true);
  ASSERT_GE(index, 0);
  EXPECT_EQ(platform_.cpu.decode_cache, nullptr);

  PlatformRemoveMemoryWatchpoint(&platform_, index);
  EXPECT_EQ(platform_.cpu.decode_cache, platform_.cpu_decode_cache);
}

TEST_F(PlatformDecodeCacheTest, NoDecodeIsReusedWhileAWatchpointIsEnabled) {
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x11});
  RunAt(kProgramOffset);
  ASSERT_EQ(al(), 0x11);

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(&platform_, 0x0200, 0x0200, true, true), 0);
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x22});
  RunAt(kProgramOffset);
  EXPECT_EQ(al(), 0x22);
}

}  // namespace
