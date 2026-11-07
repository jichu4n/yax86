#include <vector>

#include "gtest/gtest.h"
#include "hdc.h"

namespace {

// Small enough that a whole image fits in the fixture, and that running off
// the end of the drive is easy to arrange. 8 sectors of 512 bytes.
constexpr YHDCDriveGeometry kTestGeometry = {
    /*num_cylinders=*/2,
    /*num_heads=*/2,
    /*num_sectors_per_track=*/2,
};
constexpr uint32_t kTestImageSize = 2 * 2 * 2 * Y_HDC_SECTOR_SIZE;

class HDCTransferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Fill the image with a pattern that identifies the offset a byte came
    // from, so a transfer reading the wrong sector is obvious.
    for (uint32_t i = 0; i < kTestImageSize; ++i) {
      image_[i] = PatternAt(i);
    }

    hdc_.config.context = this;
    hdc_.config.read_image_byte = [](void* context, uint8_t drive,
                                     uint32_t offset) -> uint8_t {
      HDCTransferTest* test = static_cast<HDCTransferTest*>(context);
      test->read_offsets_.push_back(offset);
      test->last_drive_ = drive;
      // The controller must never ask for a byte off the end of the drive.
      EXPECT_LT(offset, kTestImageSize);
      return offset < kTestImageSize ? test->image_[offset] : 0xFF;
    };
    hdc_.config.write_image_byte = [](void* context, uint8_t drive,
                                      uint32_t offset, uint8_t value) {
      HDCTransferTest* test = static_cast<HDCTransferTest*>(context);
      test->last_drive_ = drive;
      EXPECT_LT(offset, kTestImageSize);
      if (offset < kTestImageSize) {
        test->image_[offset] = value;
        test->written_offsets_.push_back(offset);
      }
    };

    YHDCInit(&hdc_);
    YHDCAttachDrive(&hdc_, 0, &kTestGeometry);
    SelectDrive(0);
  }

  static uint8_t PatternAt(uint32_t offset) {
    return static_cast<uint8_t>((offset * 7 + 3) & 0xFF);
  }

  uint8_t Read(YHDCRegister reg) {
    return YHDCReadPort(
        &hdc_, Y_HDC_PORT_BASE + YHDCPortOffsetToRegister((uint8_t)reg));
  }

  void Write(YHDCRegister reg, uint8_t value) {
    YHDCWritePort(
        &hdc_, Y_HDC_PORT_BASE + YHDCPortOffsetToRegister((uint8_t)reg), value);
  }

  void SelectDrive(uint8_t drive) {
    Write(
        Y_HDC_REGISTER_DRIVE_HEAD,
        drive == 0 ? 0 : (uint8_t)Y_HDC_DRIVE_HEAD_DRIVE_SELECT);
  }

  // Addresses a sector by cylinder/head/sector, sectors numbered from one.
  void SetChsAddress(
      uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count) {
    Write(Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)(cylinder & 0xFF));
    Write(Y_HDC_REGISTER_CYLINDER_HIGH, (uint8_t)(cylinder >> 8));
    Write(Y_HDC_REGISTER_DRIVE_HEAD, head);
    Write(Y_HDC_REGISTER_SECTOR_NUMBER, sector);
    Write(Y_HDC_REGISTER_SECTOR_COUNT, count);
  }

  void SetLbaAddress(uint32_t lba, uint8_t count) {
    Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)(lba & 0xFF));
    Write(Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)((lba >> 8) & 0xFF));
    Write(Y_HDC_REGISTER_CYLINDER_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    Write(
        Y_HDC_REGISTER_DRIVE_HEAD,
        (uint8_t)(Y_HDC_DRIVE_HEAD_LBA | ((lba >> 24) & 0x0F)));
    Write(Y_HDC_REGISTER_SECTOR_COUNT, count);
  }

  uint8_t RunCommand(uint8_t opcode) {
    Write(Y_HDC_REGISTER_STATUS, opcode);
    return Read(Y_HDC_REGISTER_STATUS);
  }

  // Drains bytes out of the data register, alternating the two data ports the
  // way a 16-bit transfer over an 8-bit bus does.
  std::vector<uint8_t> Drain(int num_bytes) {
    std::vector<uint8_t> data;
    for (int i = 0; i < num_bytes; ++i) {
      data.push_back(
          Read(i % 2 == 0 ? Y_HDC_REGISTER_DATA : Y_HDC_REGISTER_DATA_HIGH));
    }
    return data;
  }

  // Pushes bytes into the data register the way the card is driven on a write:
  // the high byte of each word goes to the latch first, and the write of the
  // low byte commits the pair.
  void Fill(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
      Write(Y_HDC_REGISTER_DATA_HIGH, data[i + 1]);
      Write(Y_HDC_REGISTER_DATA, data[i]);
    }
  }
  YHDCState hdc_ = {0};
  uint8_t image_[kTestImageSize] = {0};
  std::vector<uint32_t> read_offsets_;
  std::vector<uint32_t> written_offsets_;
  uint8_t last_drive_ = 0xFF;
};

TEST_F(HDCTransferTest, ReadsASingleSector) {
  // Cylinder 1, head 0, sector 2 - LBA 3 with this geometry.
  SetChsAddress(1, 0, 2, 1);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  ASSERT_TRUE(status & Y_HDC_STATUS_DATA_REQUEST);
  ASSERT_FALSE(status & Y_HDC_STATUS_ERROR);

  const std::vector<uint8_t> data = Drain(Y_HDC_SECTOR_SIZE);
  for (int i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
    ASSERT_EQ(data[i], PatternAt(3 * Y_HDC_SECTOR_SIZE + i)) << "byte " << i;
  }
  // The transfer ends when the sector is drained.
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 0);
}

TEST_F(HDCTransferTest, ReadsConsecutiveSectorsInOneCommand) {
  SetLbaAddress(2, 3);
  ASSERT_TRUE(
      RunCommand(Y_HDC_COMMAND_READ_SECTORS) & Y_HDC_STATUS_DATA_REQUEST);

  const std::vector<uint8_t> data = Drain(3 * Y_HDC_SECTOR_SIZE);
  for (int i = 0; i < 3 * Y_HDC_SECTOR_SIZE; ++i) {
    ASSERT_EQ(data[i], PatternAt(2 * Y_HDC_SECTOR_SIZE + i)) << "byte " << i;
  }
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);

  // Exactly the bytes of sectors 2, 3 and 4, in order.
  ASSERT_EQ(read_offsets_.size(), 3u * Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(read_offsets_.front(), 2u * Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(read_offsets_.back(), 5u * Y_HDC_SECTOR_SIZE - 1);
}

TEST_F(HDCTransferTest, DataRequestStaysAssertedAcrossASectorBoundary) {
  SetLbaAddress(0, 2);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);

  // A read of the low byte port takes a whole word off the card, so two
  // sectors are a sector's worth of words apiece rather than of reads.
  const int num_words = 2 * Y_HDC_SECTOR_SIZE / 2;
  for (int i = 0; i < num_words - 1; ++i) {
    Read(Y_HDC_REGISTER_DATA);
    Read(Y_HDC_REGISTER_DATA_HIGH);
    ASSERT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST)
        << "cleared early after " << (i + 1) << " words";
  }
  Read(Y_HDC_REGISTER_DATA);
  Read(Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
}

TEST_F(HDCTransferTest, SectorCountCountsDownAsSectorsComplete) {
  SetLbaAddress(0, 3);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 3);

  Drain(Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 2);
  Drain(Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 1);
  Drain(Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 0);
}

TEST_F(HDCTransferTest, WritesASingleSector) {
  std::vector<uint8_t> data;
  for (int i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
    data.push_back(static_cast<uint8_t>(0xA0 + (i & 0x0F)));
  }

  SetLbaAddress(5, 1);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_WRITE_SECTORS);
  ASSERT_TRUE(status & Y_HDC_STATUS_DATA_REQUEST);
  ASSERT_FALSE(status & Y_HDC_STATUS_ERROR);

  Fill(data);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);

  for (int i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
    ASSERT_EQ(image_[5 * Y_HDC_SECTOR_SIZE + i], data[i]) << "byte " << i;
  }
  // Neighbouring sectors are untouched.
  EXPECT_EQ(
      image_[5 * Y_HDC_SECTOR_SIZE - 1], PatternAt(5 * Y_HDC_SECTOR_SIZE - 1));
  EXPECT_EQ(image_[6 * Y_HDC_SECTOR_SIZE], PatternAt(6 * Y_HDC_SECTOR_SIZE));
}

TEST_F(HDCTransferTest, WordsLandOnTheDiskInTheRightByteOrder) {
  // Reads and writes drive the two data ports in opposite orders, so streaming
  // bytes in the order they arrive puts every word on the disk back to front.
  // A partition table's 0xAA55 signature ends up as 0x55AA, and DOS sees no
  // partition at all.
  SetLbaAddress(0, 1);
  RunCommand(Y_HDC_COMMAND_WRITE_SECTORS);

  // The last word of a boot sector, written high byte first.
  std::vector<uint8_t> sector(Y_HDC_SECTOR_SIZE, 0);
  sector[Y_HDC_SECTOR_SIZE - 2] = 0x55;
  sector[Y_HDC_SECTOR_SIZE - 1] = 0xAA;
  Fill(sector);

  EXPECT_EQ(image_[Y_HDC_SECTOR_SIZE - 2], 0x55);
  EXPECT_EQ(image_[Y_HDC_SECTOR_SIZE - 1], 0xAA);
}

TEST_F(HDCTransferTest, WrittenSectorsReadBack) {
  std::vector<uint8_t> data(2 * Y_HDC_SECTOR_SIZE);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i * 3 + 1);
  }

  SetLbaAddress(1, 2);
  RunCommand(Y_HDC_COMMAND_WRITE_SECTORS);
  Fill(data);

  SetLbaAddress(1, 2);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  EXPECT_EQ(Drain(2 * Y_HDC_SECTOR_SIZE), data);
}

TEST_F(HDCTransferTest, ChsAndLbaAddressTheSameSector) {
  // Cylinder 1, head 1, sector 1 is LBA 6 with this geometry.
  SetChsAddress(1, 1, 1, 1);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  const std::vector<uint8_t> via_chs = Drain(Y_HDC_SECTOR_SIZE);

  SetLbaAddress(6, 1);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  const std::vector<uint8_t> via_lba = Drain(Y_HDC_SECTOR_SIZE);

  EXPECT_EQ(via_chs, via_lba);
  EXPECT_EQ(via_chs[0], PatternAt(6 * Y_HDC_SECTOR_SIZE));
}

TEST_F(HDCTransferTest, ReadPastTheEndOfTheDriveFails) {
  // The last sector is LBA 7, so LBA 8 is off the drive.
  SetLbaAddress(8, 1);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  EXPECT_TRUE(status & Y_HDC_STATUS_ERROR);
  EXPECT_FALSE(status & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, MultiSectorRunPastTheEndOfTheDriveFails) {
  // Starts on the drive but runs off the end, which a check of only the first
  // sector would let through.
  SetLbaAddress(6, 4);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_READ_SECTORS);
  EXPECT_TRUE(status & Y_HDC_STATUS_ERROR);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, WritePastTheEndOfTheDriveFails) {
  SetLbaAddress(8, 1);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_WRITE_SECTORS);
  EXPECT_TRUE(status & Y_HDC_STATUS_ERROR);
  EXPECT_FALSE(status & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND);
  EXPECT_TRUE(written_offsets_.empty());
}

TEST_F(HDCTransferTest, WriteWithNoTransferActiveIsDiscarded) {
  Write(Y_HDC_REGISTER_DATA, 0xFF);
  EXPECT_TRUE(written_offsets_.empty());
}

TEST_F(HDCTransferTest, ReadDuringAWriteReturnsZero) {
  SetLbaAddress(0, 1);
  RunCommand(Y_HDC_COMMAND_WRITE_SECTORS);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DATA), 0);
  // The transfer is not disturbed by the stray read.
  EXPECT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, TransfersUseTheDriveSelectedWhenTheCommandStarted) {
  YHDCAttachDrive(&hdc_, 1, &kTestGeometry);
  SelectDrive(0);
  SetLbaAddress(0, 1);
  RunCommand(Y_HDC_COMMAND_READ_SECTORS);

  // Selecting the other drive mid-transfer must not redirect the bytes still
  // in flight.
  SelectDrive(1);
  Drain(Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(last_drive_, 0);
}

TEST_F(HDCTransferTest, ADriveWithNoImageReadsAsZeroes) {
  hdc_.config.read_image_byte = nullptr;
  hdc_.config.write_image_byte = nullptr;
  YHDCInit(&hdc_);
  YHDCAttachDrive(&hdc_, 0, &kTestGeometry);
  SelectDrive(0);

  SetLbaAddress(0, 1);
  ASSERT_TRUE(
      RunCommand(Y_HDC_COMMAND_READ_SECTORS) & Y_HDC_STATUS_DATA_REQUEST);
  const std::vector<uint8_t> data = Drain(Y_HDC_SECTOR_SIZE);
  EXPECT_EQ(data, std::vector<uint8_t>(Y_HDC_SECTOR_SIZE, 0));
}

TEST_F(HDCTransferTest, ASectorCountOfZeroMeansTheLargestTransfer) {
  // ATA encodes 256 sectors as a count of zero. This drive is far smaller than
  // that, so the run does not fit and the command has to fail rather than read
  // a single sector.
  SetLbaAddress(0, 0);
  EXPECT_TRUE(RunCommand(Y_HDC_COMMAND_READ_SECTORS) & Y_HDC_STATUS_ERROR);
  EXPECT_TRUE(read_offsets_.empty());
}

TEST_F(HDCTransferTest, ReadVerifyDoesNotTouchTheImage) {
  SetLbaAddress(0, 1);
  EXPECT_FALSE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_TRUE(read_offsets_.empty());
}

}  // namespace
