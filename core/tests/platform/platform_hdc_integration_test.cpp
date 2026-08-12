#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformHDCIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);  // 64KB RAM
    config_.context = this;
    config_.physical_memory = ram_;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024] = {0};
};

TEST_F(PlatformHDCIntegrationTest, OptionROMIsVisibleWhereTheBIOSScans) {
  // The BIOS looks for the signature at the start of the ROM, then reads its
  // size from the third byte.
  EXPECT_EQ(
      ReadMemoryWord(&platform_, kHDCOptionROMStartAddress), (uint16_t)0xAA55);
  EXPECT_EQ(
      ReadMemoryByte(&platform_, kHDCOptionROMStartAddress + 2) * 512,
      HDCGetOptionROMSize());

  // Every byte of the ROM is reachable through the memory map, and matches
  // what the module reports directly.
  for (uint32_t offset = 0; offset < HDCGetOptionROMSize(); offset += 256) {
    ASSERT_EQ(
        ReadMemoryByte(&platform_, kHDCOptionROMStartAddress + offset),
        HDCReadOptionROMByte(offset))
        << "at offset " << offset;
  }
  const uint32_t last = HDCGetOptionROMSize() - 1;
  EXPECT_EQ(
      ReadMemoryByte(&platform_, kHDCOptionROMStartAddress + last),
      HDCReadOptionROMByte(last));
}

TEST_F(PlatformHDCIntegrationTest, OptionROMIsMappedAsItsOwnRegion) {
  MemoryMapEntry* entry =
      GetMemoryMapEntryByType(&platform_, kMemoryMapEntryHDCOptionROM);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->start, (uint32_t)kHDCOptionROMStartAddress);
  EXPECT_EQ(entry->end, kHDCOptionROMStartAddress + HDCGetOptionROMSize() - 1);
  EXPECT_EQ(
      GetMemoryMapEntryForAddress(&platform_, kHDCOptionROMStartAddress),
      entry);
}

TEST_F(PlatformHDCIntegrationTest, OptionROMIsReadOnly) {
  const uint8_t before = ReadMemoryByte(&platform_, kHDCOptionROMStartAddress);
  WriteMemoryByte(&platform_, kHDCOptionROMStartAddress, 0x00);
  EXPECT_EQ(ReadMemoryByte(&platform_, kHDCOptionROMStartAddress), before);
}

}  // namespace
