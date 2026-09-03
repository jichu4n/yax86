#include "gtest/gtest.h"
#include "platform.h"

namespace {

enum {
  // A type of our own, distinct from every type the platform registers itself.
  kMemoryMapEntryTest = 0x7F,
};

class PlatformMemoryMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);  // 64KB RAM
    config_.context = this;
    config_.physical_memory = ram_;
    config_.vram = vram_;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  // Registers a region backed by region_ over [start, end].
  bool RegisterTestRegion(uint32_t start, uint32_t end) {
    MemoryMapEntry entry = {0};
    entry.context = this;
    entry.entry_type = kMemoryMapEntryTest;
    entry.start = start;
    entry.end = end;
    entry.read_data = region_;
    entry.write_data = region_;
    return RegisterMemoryMapEntry(&platform_, &entry);
  }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
  uint8_t region_[3 * kMemoryPageSize] = {0};
};

TEST_F(PlatformMemoryMapTest, ResolvesConventionalMemory) {
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(&platform_, 0x00400);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->entry_type, (MemoryMapEntryType)kMemoryMapEntryConventional);
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, sizeof(ram_) - 1), entry);
}

TEST_F(PlatformMemoryMapTest, UnmappedAddressResolvesToNothing) {
  // Just past the 64KB of RAM this machine has, and well below the BIOS.
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, sizeof(ram_)), nullptr);
}

TEST_F(PlatformMemoryMapTest, AddressAboveTheAddressSpaceResolvesToNothing) {
  // No entry may reach above the first megabyte, so an address there is
  // unmapped by definition rather than something the map has to be walked for.
  EXPECT_EQ(
      GetMemoryMapEntryForAddress(&platform_, kMemoryAddressSpaceSize),
      nullptr);
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, 0xFFFFFFFF), nullptr);
}

// The index has a slot per page of the address space and none above it, so an
// entry reaching past the top could not be recorded in it. That it cannot be
// registered at all is what lets a lookup treat the index as the whole answer.
TEST_F(PlatformMemoryMapTest, RegionReachingPastTheAddressSpaceIsRejected) {
  EXPECT_FALSE(RegisterTestRegion(
      kMemoryAddressSpaceSize - kMemoryPageSize,
      kMemoryAddressSpaceSize + kMemoryPageSize - 1));
  EXPECT_FALSE(RegisterTestRegion(
      kMemoryAddressSpaceSize, kMemoryAddressSpaceSize + kMemoryPageSize - 1));
  EXPECT_EQ(
      GetMemoryMapEntryByType(
          &platform_, (MemoryMapEntryType)kMemoryMapEntryTest),
      nullptr);
}

TEST_F(PlatformMemoryMapTest, PageAlignedRegionResolvesThroughout) {
  const uint32_t start = 0xD0000;
  const uint32_t end = start + sizeof(region_) - 1;
  ASSERT_TRUE(RegisterTestRegion(start, end));

  MemoryMapEntry* entry = GetMemoryMapEntryByType(
      &platform_, (MemoryMapEntryType)kMemoryMapEntryTest);
  ASSERT_NE(entry, nullptr);
  for (uint32_t address = start; address <= end; address += 256) {
    ASSERT_EQ(GetMemoryMapEntryForAddress(&platform_, address), entry)
        << "at " << std::hex << address;
  }
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, start - 1), nullptr);
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, end + 1), nullptr);
}

// The page index cannot answer for a page more than one entry has a share of,
// so it marks those pages and the lookup falls back to walking the map.
// Nothing the platform registers today is misaligned, which is exactly why
// this needs a test of its own: the fallback would otherwise never run.
TEST_F(PlatformMemoryMapTest, RegionStraddlingPageBoundariesResolvesExactly) {
  const uint32_t start = 0xD0000 + 0x800;
  const uint32_t end = start + kMemoryPageSize;  // Spans three pages, partly.
  ASSERT_TRUE(RegisterTestRegion(start, end));

  MemoryMapEntry* entry = GetMemoryMapEntryByType(
      &platform_, (MemoryMapEntryType)kMemoryMapEntryTest);
  ASSERT_NE(entry, nullptr);

  // Every byte inside resolves to the region, including the two partial pages
  // at either end.
  for (uint32_t address = start; address <= end; ++address) {
    ASSERT_EQ(GetMemoryMapEntryForAddress(&platform_, address), entry)
        << "at " << std::hex << address;
  }
  // And nothing outside it does, including the rest of those same pages.
  for (uint32_t address = start - 0x800; address < start; ++address) {
    ASSERT_EQ(GetMemoryMapEntryForAddress(&platform_, address), nullptr)
        << "at " << std::hex << address;
  }
  for (uint32_t address = end + 1; address < end + 0x800; ++address) {
    ASSERT_EQ(GetMemoryMapEntryForAddress(&platform_, address), nullptr)
        << "at " << std::hex << address;
  }
}

TEST_F(PlatformMemoryMapTest, StraddlingRegionReadsAndWritesTheRightBytes) {
  const uint32_t start = 0xD0000 + 0x800;
  const uint32_t end = start + kMemoryPageSize;
  ASSERT_TRUE(RegisterTestRegion(start, end));

  WriteMemoryByte(&platform_, start, 0x11);
  WriteMemoryByte(&platform_, start + kMemoryPageSize / 2, 0x22);
  WriteMemoryByte(&platform_, end, 0x33);

  EXPECT_EQ(region_[0], 0x11);
  EXPECT_EQ(region_[kMemoryPageSize / 2], 0x22);
  EXPECT_EQ(region_[kMemoryPageSize], 0x33);

  EXPECT_EQ(ReadMemoryByte(&platform_, start), 0x11);
  EXPECT_EQ(ReadMemoryByte(&platform_, start + kMemoryPageSize / 2), 0x22);
  EXPECT_EQ(ReadMemoryByte(&platform_, end), 0x33);
}

// A later registration must be reflected in the index, not just in the map.
TEST_F(PlatformMemoryMapTest, IndexTracksRegistrationsAfterInit) {
  const uint32_t start = 0xD0000;
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, start), nullptr);
  ASSERT_TRUE(RegisterTestRegion(start, start + sizeof(region_) - 1));
  EXPECT_NE(GetMemoryMapEntryForAddress(&platform_, start), nullptr);
}

// An entry may end at the very top of the address space, which is the last slot
// the index has. The BIOS ROM is that entry on every machine the platform
// builds.
TEST_F(PlatformMemoryMapTest, TopPageOfTheAddressSpaceIsIndexed) {
  const uint32_t top = kMemoryAddressSpaceSize - 1;
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(&platform_, top);
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->end, top);

  // The whole of that last page resolves to it, and nothing above it does.
  EXPECT_EQ(
      GetMemoryMapEntryForAddress(&platform_, top - kMemoryPageSize + 1),
      entry);
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, top + 1), nullptr);
}

TEST_F(PlatformMemoryMapTest, OverlappingRegistrationIsRejected) {
  // Conventional memory already covers this, so the index must be left alone.
  MemoryMapEntry entry = {0};
  entry.entry_type = kMemoryMapEntryTest;
  entry.start = 0x00000;
  entry.end = 0x00FFF;
  entry.read_data = region_;
  entry.write_data = region_;
  EXPECT_FALSE(RegisterMemoryMapEntry(&platform_, &entry));

  MemoryMapEntry* conventional =
      GetMemoryMapEntryByType(&platform_, kMemoryMapEntryConventional);
  EXPECT_EQ(GetMemoryMapEntryForAddress(&platform_, 0x00000), conventional);
}

}  // namespace
