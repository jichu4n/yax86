#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

constexpr uint16_t kProgramOffset = 0x0100;

enum : uint8_t {
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
};

// Reusing a decoded instruction is only safe while everything that changes
// what is at an address says so. The platform is where most of those changes
// happen - DMA writes, and a region being mapped.
class PlatformDecodeCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    platform_.config.physical_memory_size = sizeof(ram_);
    platform_.config.context = this;
    platform_.config.physical_memory = ram_;
    platform_.config.vram = vram_;
    platform_.config.video_adapter = kVideoAdapterCGA;

    ASSERT_TRUE(PlatformInit(&platform_));
    platform_.cpu.registers[kCS] = 0;
    platform_.cpu.registers[kDS] = 0;
    platform_.cpu.registers[kSS] = 0;
    platform_.cpu.registers[kSP] = 0xFFFE;
  }

  // Writes straight into the host's buffer, which is what makes a reused
  // decode visible: nothing about this write reaches the CPU.
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

  PlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
  uint8_t rom_[0x800] = {0};
};

TEST_F(PlatformDecodeCacheTest, ThePlatformHandsTheCPUItsDecodeCache) {
  EXPECT_EQ(platform_.cpu.config.decode_cache, platform_.cpu_decode_cache);
  EXPECT_EQ(platform_.cpu.config.decode_cache_num_entries, kDecodeCacheEntries);
  // Which CPUInit() accepted, rather than logging and running without one.
  EXPECT_EQ(platform_.cpu.decode_cache_index_mask, kDecodeCacheEntries - 1);
}

// The path DMA takes. DOS loads itself over the boot sector this way, so an
// unreported write here is the difference between booting and not.
TEST_F(PlatformDecodeCacheTest, AWriteThroughTheMemoryMapDiscardsTheDecode) {
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x11});
  RunAt(kProgramOffset);
  ASSERT_EQ(al(), 0x11);

  WriteMemoryByte(&platform_, kProgramOffset + 1, 0x22);
  RunAt(kProgramOffset);
  EXPECT_EQ(al(), 0x22);
}

// Nothing was written - what changed is which region owns the address, so the
// page generations have nothing to say and the whole cache goes.
TEST_F(PlatformDecodeCacheTest, RegisteringAMemoryRegionDiscardsEveryDecode) {
  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x11});
  RunAt(kProgramOffset);
  ASSERT_EQ(al(), 0x11);

  PokeBehindTheCPUsBack(kProgramOffset, {kOpMovAlImm8, 0x22});
  // Any type no module has claimed, mapped somewhere nothing else covers.
  MemoryMapEntry extra_rom = {};
  extra_rom.entry_type = 0x7F;
  extra_rom.start = 0xD0000;
  extra_rom.end = 0xD07FF;
  extra_rom.read_data = rom_;
  ASSERT_TRUE(RegisterMemoryMapEntry(&platform_, &extra_rom));
  PlatformUpdateAfterMemoryMapChange(&platform_);

  RunAt(kProgramOffset);
  EXPECT_EQ(al(), 0x22);
}

}  // namespace
