#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformHDCIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = 64 * 1024;  // 64KB RAM
    config_.context = this;
    config_.read_physical_memory_byte = [](PlatformState* p,
                                           uint32_t addr) -> uint8_t {
      PlatformHDCIntegrationTest* test =
          static_cast<PlatformHDCIntegrationTest*>(p->config->context);
      if (addr < sizeof(test->ram_)) return test->ram_[addr];
      return 0xFF;
    };
    config_.write_physical_memory_byte = [](PlatformState* p, uint32_t addr,
                                            uint8_t val) {
      PlatformHDCIntegrationTest* test =
          static_cast<PlatformHDCIntegrationTest*>(p->config->context);
      if (addr < sizeof(test->ram_)) test->ram_[addr] = val;
    };

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024];
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
