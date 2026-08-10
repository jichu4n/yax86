#include <vector>

#include "gtest/gtest.h"
#include "hdc.h"

namespace {

// Small enough that a whole image fits in the fixture, and that running off
// the end of the drive is easy to arrange. 8 sectors of 512 bytes.
constexpr HDCDriveGeometry kTestGeometry = {
    /*num_cylinders=*/2,
    /*num_heads=*/2,
    /*num_sectors_per_track=*/2,
};
constexpr uint32_t kTestImageSize = 2 * 2 * 2 * kHDCSectorSize;

class HDCTransferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Fill the image with a pattern that identifies the offset a byte came
    // from, so a transfer reading the wrong sector is obvious.
    for (uint32_t i = 0; i < kTestImageSize; ++i) {
      image_[i] = PatternAt(i);
    }

    config_.context = this;
    config_.read_image_byte = [](void* context, uint8_t drive,
                                 uint32_t offset) -> uint8_t {
      HDCTransferTest* test = static_cast<HDCTransferTest*>(context);
      test->read_offsets_.push_back(offset);
      test->last_drive_ = drive;
      // The controller must never ask for a byte off the end of the drive.
      EXPECT_LT(offset, kTestImageSize);
      return offset < kTestImageSize ? test->image_[offset] : 0xFF;
    };
    config_.write_image_byte = [](void* context, uint8_t drive, uint32_t offset,
                                  uint8_t value) {
      HDCTransferTest* test = static_cast<HDCTransferTest*>(context);
      test->last_drive_ = drive;
      EXPECT_LT(offset, kTestImageSize);
      if (offset < kTestImageSize) {
        test->image_[offset] = value;
        test->written_offsets_.push_back(offset);
      }
    };

    HDCInit(&hdc_, &config_);
    HDCAttachDrive(&hdc_, 0, &kTestGeometry);
    SelectDrive(0);
  }

  static uint8_t PatternAt(uint32_t offset) {
    return static_cast<uint8_t>((offset * 7 + 3) & 0xFF);
  }

  uint8_t Read(HDCRegister reg) {
    return HDCReadPort(
        &hdc_, kHDCPortBase + HDCPortOffsetToRegister((uint8_t)reg));
  }

  void Write(HDCRegister reg, uint8_t value) {
    HDCWritePort(
        &hdc_, kHDCPortBase + HDCPortOffsetToRegister((uint8_t)reg), value);
  }

  void SelectDrive(uint8_t drive) {
    Write(
        kHDCRegisterDriveHead,
        drive == 0 ? 0 : (uint8_t)kHDCDriveHeadDriveSelect);
  }

  // Addresses a sector by cylinder/head/sector, sectors numbered from one.
  void SetChsAddress(
      uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count) {
    Write(kHDCRegisterCylinderLow, (uint8_t)(cylinder & 0xFF));
    Write(kHDCRegisterCylinderHigh, (uint8_t)(cylinder >> 8));
    Write(kHDCRegisterDriveHead, head);
    Write(kHDCRegisterSectorNumber, sector);
    Write(kHDCRegisterSectorCount, count);
  }

  void SetLbaAddress(uint32_t lba, uint8_t count) {
    Write(kHDCRegisterSectorNumber, (uint8_t)(lba & 0xFF));
    Write(kHDCRegisterCylinderLow, (uint8_t)((lba >> 8) & 0xFF));
    Write(kHDCRegisterCylinderHigh, (uint8_t)((lba >> 16) & 0xFF));
    Write(
        kHDCRegisterDriveHead,
        (uint8_t)(kHDCDriveHeadLBA | ((lba >> 24) & 0x0F)));
    Write(kHDCRegisterSectorCount, count);
  }

  uint8_t RunCommand(uint8_t opcode) {
    Write(kHDCRegisterStatus, opcode);
    return Read(kHDCRegisterStatus);
  }

  // Drains bytes out of the data register, alternating the two data ports the
  // way a 16-bit transfer over an 8-bit bus does.
  std::vector<uint8_t> Drain(int num_bytes) {
    std::vector<uint8_t> data;
    for (int i = 0; i < num_bytes; ++i) {
      data.push_back(
          Read(i % 2 == 0 ? kHDCRegisterData : kHDCRegisterDataHigh));
    }
    return data;
  }

  // Pushes bytes into the data register the way the card is driven on a write:
  // the high byte of each word goes to the latch first, and the write of the
  // low byte commits the pair.
  void Fill(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
      Write(kHDCRegisterDataHigh, data[i + 1]);
      Write(kHDCRegisterData, data[i]);
    }
  }

  HDCConfig config_ = {0};
  HDCState hdc_ = {0};
  uint8_t image_[kTestImageSize] = {0};
  std::vector<uint32_t> read_offsets_;
  std::vector<uint32_t> written_offsets_;
  uint8_t last_drive_ = 0xFF;
};

TEST_F(HDCTransferTest, ReadsASingleSector) {
  // Cylinder 1, head 0, sector 2 - LBA 3 with this geometry.
  SetChsAddress(1, 0, 2, 1);
  const uint8_t status = RunCommand(kHDCCommandReadSectors);
  ASSERT_TRUE(status & kHDCStatusDataRequest);
  ASSERT_FALSE(status & kHDCStatusError);

  const std::vector<uint8_t> data = Drain(kHDCSectorSize);
  for (int i = 0; i < kHDCSectorSize; ++i) {
    ASSERT_EQ(data[i], PatternAt(3 * kHDCSectorSize + i)) << "byte " << i;
  }
  // The transfer ends when the sector is drained.
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);
  EXPECT_EQ(Read(kHDCRegisterSectorCount), 0);
}

TEST_F(HDCTransferTest, ReadsConsecutiveSectorsInOneCommand) {
  SetLbaAddress(2, 3);
  ASSERT_TRUE(RunCommand(kHDCCommandReadSectors) & kHDCStatusDataRequest);

  const std::vector<uint8_t> data = Drain(3 * kHDCSectorSize);
  for (int i = 0; i < 3 * kHDCSectorSize; ++i) {
    ASSERT_EQ(data[i], PatternAt(2 * kHDCSectorSize + i)) << "byte " << i;
  }
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);

  // Exactly the bytes of sectors 2, 3 and 4, in order.
  ASSERT_EQ(read_offsets_.size(), 3u * kHDCSectorSize);
  EXPECT_EQ(read_offsets_.front(), 2u * kHDCSectorSize);
  EXPECT_EQ(read_offsets_.back(), 5u * kHDCSectorSize - 1);
}

TEST_F(HDCTransferTest, DataRequestStaysAssertedAcrossASectorBoundary) {
  SetLbaAddress(0, 2);
  RunCommand(kHDCCommandReadSectors);

  // A read of the low byte port takes a whole word off the card, so two
  // sectors are a sector's worth of words apiece rather than of reads.
  const int num_words = 2 * kHDCSectorSize / 2;
  for (int i = 0; i < num_words - 1; ++i) {
    Read(kHDCRegisterData);
    Read(kHDCRegisterDataHigh);
    ASSERT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest)
        << "cleared early after " << (i + 1) << " words";
  }
  Read(kHDCRegisterData);
  Read(kHDCRegisterDataHigh);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);
}

TEST_F(HDCTransferTest, SectorCountCountsDownAsSectorsComplete) {
  SetLbaAddress(0, 3);
  RunCommand(kHDCCommandReadSectors);
  EXPECT_EQ(Read(kHDCRegisterSectorCount), 3);

  Drain(kHDCSectorSize);
  EXPECT_EQ(Read(kHDCRegisterSectorCount), 2);
  Drain(kHDCSectorSize);
  EXPECT_EQ(Read(kHDCRegisterSectorCount), 1);
  Drain(kHDCSectorSize);
  EXPECT_EQ(Read(kHDCRegisterSectorCount), 0);
}

TEST_F(HDCTransferTest, WritesASingleSector) {
  std::vector<uint8_t> data;
  for (int i = 0; i < kHDCSectorSize; ++i) {
    data.push_back(static_cast<uint8_t>(0xA0 + (i & 0x0F)));
  }

  SetLbaAddress(5, 1);
  const uint8_t status = RunCommand(kHDCCommandWriteSectors);
  ASSERT_TRUE(status & kHDCStatusDataRequest);
  ASSERT_FALSE(status & kHDCStatusError);

  Fill(data);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);

  for (int i = 0; i < kHDCSectorSize; ++i) {
    ASSERT_EQ(image_[5 * kHDCSectorSize + i], data[i]) << "byte " << i;
  }
  // Neighbouring sectors are untouched.
  EXPECT_EQ(image_[5 * kHDCSectorSize - 1], PatternAt(5 * kHDCSectorSize - 1));
  EXPECT_EQ(image_[6 * kHDCSectorSize], PatternAt(6 * kHDCSectorSize));
}

TEST_F(HDCTransferTest, WordsLandOnTheDiskInTheRightByteOrder) {
  // Reads and writes drive the two data ports in opposite orders, so streaming
  // bytes in the order they arrive puts every word on the disk back to front.
  // A partition table's 0xAA55 signature ends up as 0x55AA, and DOS sees no
  // partition at all.
  SetLbaAddress(0, 1);
  RunCommand(kHDCCommandWriteSectors);

  // The last word of a boot sector, written high byte first.
  std::vector<uint8_t> sector(kHDCSectorSize, 0);
  sector[kHDCSectorSize - 2] = 0x55;
  sector[kHDCSectorSize - 1] = 0xAA;
  Fill(sector);

  EXPECT_EQ(image_[kHDCSectorSize - 2], 0x55);
  EXPECT_EQ(image_[kHDCSectorSize - 1], 0xAA);
}

TEST_F(HDCTransferTest, WrittenSectorsReadBack) {
  std::vector<uint8_t> data(2 * kHDCSectorSize);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i * 3 + 1);
  }

  SetLbaAddress(1, 2);
  RunCommand(kHDCCommandWriteSectors);
  Fill(data);

  SetLbaAddress(1, 2);
  RunCommand(kHDCCommandReadSectors);
  EXPECT_EQ(Drain(2 * kHDCSectorSize), data);
}

TEST_F(HDCTransferTest, ChsAndLbaAddressTheSameSector) {
  // Cylinder 1, head 1, sector 1 is LBA 6 with this geometry.
  SetChsAddress(1, 1, 1, 1);
  RunCommand(kHDCCommandReadSectors);
  const std::vector<uint8_t> via_chs = Drain(kHDCSectorSize);

  SetLbaAddress(6, 1);
  RunCommand(kHDCCommandReadSectors);
  const std::vector<uint8_t> via_lba = Drain(kHDCSectorSize);

  EXPECT_EQ(via_chs, via_lba);
  EXPECT_EQ(via_chs[0], PatternAt(6 * kHDCSectorSize));
}

TEST_F(HDCTransferTest, ReadPastTheEndOfTheDriveFails) {
  // The last sector is LBA 7, so LBA 8 is off the drive.
  SetLbaAddress(8, 1);
  const uint8_t status = RunCommand(kHDCCommandReadSectors);
  EXPECT_TRUE(status & kHDCStatusError);
  EXPECT_FALSE(status & kHDCStatusDataRequest);
  EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorIDNotFound);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, MultiSectorRunPastTheEndOfTheDriveFails) {
  // Starts on the drive but runs off the end, which a check of only the first
  // sector would let through.
  SetLbaAddress(6, 4);
  const uint8_t status = RunCommand(kHDCCommandReadSectors);
  EXPECT_TRUE(status & kHDCStatusError);
  EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorIDNotFound);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, WritePastTheEndOfTheDriveFails) {
  SetLbaAddress(8, 1);
  const uint8_t status = RunCommand(kHDCCommandWriteSectors);
  EXPECT_TRUE(status & kHDCStatusError);
  EXPECT_FALSE(status & kHDCStatusDataRequest);
  EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorIDNotFound);
  EXPECT_TRUE(written_offsets_.empty());
}

TEST_F(HDCTransferTest, WriteWithNoTransferActiveIsDiscarded) {
  Write(kHDCRegisterData, 0xFF);
  EXPECT_TRUE(written_offsets_.empty());
}

TEST_F(HDCTransferTest, ReadDuringAWriteReturnsZero) {
  SetLbaAddress(0, 1);
  RunCommand(kHDCCommandWriteSectors);
  EXPECT_EQ(Read(kHDCRegisterData), 0);
  // The transfer is not disturbed by the stray read.
  EXPECT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, TransfersUseTheDriveSelectedWhenTheCommandStarted) {
  HDCAttachDrive(&hdc_, 1, &kTestGeometry);
  SelectDrive(0);
  SetLbaAddress(0, 1);
  RunCommand(kHDCCommandReadSectors);

  // Selecting the other drive mid-transfer must not redirect the bytes still
  // in flight.
  SelectDrive(1);
  Drain(kHDCSectorSize);
  EXPECT_EQ(last_drive_, 0);
}

TEST_F(HDCTransferTest, ADriveWithNoImageReadsAsZeroes) {
  config_.read_image_byte = nullptr;
  config_.write_image_byte = nullptr;
  HDCInit(&hdc_, &config_);
  HDCAttachDrive(&hdc_, 0, &kTestGeometry);
  SelectDrive(0);

  SetLbaAddress(0, 1);
  ASSERT_TRUE(RunCommand(kHDCCommandReadSectors) & kHDCStatusDataRequest);
  const std::vector<uint8_t> data = Drain(kHDCSectorSize);
  EXPECT_EQ(data, std::vector<uint8_t>(kHDCSectorSize, 0));
}

TEST_F(HDCTransferTest, ASectorCountOfZeroMeansTheLargestTransfer) {
  // ATA encodes 256 sectors as a count of zero. This drive is far smaller than
  // that, so the run does not fit and the command has to fail rather than read
  // a single sector.
  SetLbaAddress(0, 0);
  EXPECT_TRUE(RunCommand(kHDCCommandReadSectors) & kHDCStatusError);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, ReadVerifyDoesNotTouchTheImage) {
  SetLbaAddress(0, 1);
  EXPECT_FALSE(RunCommand(kHDCCommandReadVerifySectors) & kHDCStatusError);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);
  EXPECT_TRUE(read_offsets_.empty());
}

}  // namespace
