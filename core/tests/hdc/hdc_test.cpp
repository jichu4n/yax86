#include "hdc.h"

#include "gtest/gtest.h"

namespace {

// Fields of an option ROM header, which is what the BIOS scans for.
enum {
  kOptionROMSignature0 = 0x55,
  kOptionROMSignature1 = 0xAA,
  kOptionROMBlockSize = 512,
};

class HDCOptionROMTest : public ::testing::Test {};

TEST_F(HDCOptionROMTest, LooksLikeAnOptionROM) {
  // The BIOS only calls into a ROM whose header it recognizes, so a truncated
  // or mangled ROM image would silently leave the guest with no hard disk
  // support at all rather than failing loudly.
  ASSERT_GT(HDCGetOptionROMSize(), 3u);
  const uint8_t* rom = HDCGetOptionROMData();
  EXPECT_EQ(rom[0], kOptionROMSignature0);
  EXPECT_EQ(rom[1], kOptionROMSignature1);

  // Third byte is the ROM's size in 512-byte blocks, and it has to agree with
  // how much data is actually here.
  EXPECT_EQ(rom[2] * kOptionROMBlockSize, HDCGetOptionROMSize());
}

TEST_F(HDCOptionROMTest, ChecksumIsValid) {
  // The BIOS sums the whole ROM as bytes and refuses to call into it unless
  // the total is zero.
  const uint8_t* rom = HDCGetOptionROMData();
  uint8_t sum = 0;
  for (uint32_t i = 0; i < HDCGetOptionROMSize(); ++i) {
    sum = (uint8_t)(sum + rom[i]);
  }
  EXPECT_EQ(sum, 0);
}

TEST_F(HDCOptionROMTest, StartsOnAScanBoundary) {
  // GLaBIOS scans for option ROMs on 2KB boundaries, so a ROM anywhere else
  // would never be found.
  EXPECT_EQ(kHDCOptionROMStartAddress % 2048, 0u);
}

}  // namespace
