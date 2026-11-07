#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformHDCIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    platform_.config.physical_memory_size = sizeof(ram_);  // 64KB RAM
    platform_.config.context = this;
    platform_.config.physical_memory = ram_;
    platform_.config.vram = vram_;

    ASSERT_TRUE(YPlatformInit(&platform_));
  }

  YPlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
};

TEST_F(PlatformHDCIntegrationTest, OptionROMIsVisibleWhereTheBIOSScans) {
  // The BIOS looks for the signature at the start of the ROM, then reads its
  // size from the third byte.
  EXPECT_EQ(
      YPlatformReadMemoryWord(&platform_, Y_HDC_OPTION_ROM_START_ADDRESS),
      (uint16_t)0xAA55);
  EXPECT_EQ(
      YPlatformReadMemoryByte(&platform_, Y_HDC_OPTION_ROM_START_ADDRESS + 2) *
          512,
      YHDCGetOptionROMSize());

  // Every byte of the ROM is reachable through the memory map, and matches
  // what the module reports directly.
  const uint8_t* rom = YHDCGetOptionROMData();
  for (uint32_t offset = 0; offset < YHDCGetOptionROMSize(); offset += 256) {
    ASSERT_EQ(
        YPlatformReadMemoryByte(
            &platform_, Y_HDC_OPTION_ROM_START_ADDRESS + offset),
        rom[offset])
        << "at offset " << offset;
  }
  const uint32_t last = YHDCGetOptionROMSize() - 1;
  EXPECT_EQ(
      YPlatformReadMemoryByte(
          &platform_, Y_HDC_OPTION_ROM_START_ADDRESS + last),
      rom[last]);
}

TEST_F(PlatformHDCIntegrationTest, OptionROMIsMappedAsItsOwnRegion) {
  YPlatformMemoryMapEntry* entry = YPlatformGetMemoryMapEntryByType(
      &platform_, Y_PLATFORM_MEMORY_MAP_ENTRY_HDC_OPTION_ROM);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->start, (uint32_t)Y_HDC_OPTION_ROM_START_ADDRESS);
  EXPECT_EQ(
      entry->end, Y_HDC_OPTION_ROM_START_ADDRESS + YHDCGetOptionROMSize() - 1);
  EXPECT_EQ(
      YPlatformGetMemoryMapEntryForAddress(
          &platform_, Y_HDC_OPTION_ROM_START_ADDRESS),
      entry);
}

TEST_F(PlatformHDCIntegrationTest, OptionROMIsReadOnly) {
  const uint8_t before =
      YPlatformReadMemoryByte(&platform_, Y_HDC_OPTION_ROM_START_ADDRESS);
  YPlatformWriteMemoryByte(&platform_, Y_HDC_OPTION_ROM_START_ADDRESS, 0x00);
  EXPECT_EQ(
      YPlatformReadMemoryByte(&platform_, Y_HDC_OPTION_ROM_START_ADDRESS),
      before);
}

}  // namespace
