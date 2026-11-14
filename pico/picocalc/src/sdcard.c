// The SPI mode protocol is the SD Association's Physical Layer Simplified
// Specification, section 7: the commands, their R1 responses, and the data
// tokens around each block.

#include "sdcard.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

// The SPI block the slot is wired to.
static spi_inst_t* const kSdcardSPI = spi0;

enum {
  kSdcardPinMISO = 16,
  kSdcardPinCS = 17,
  kSdcardPinSCK = 18,
  kSdcardPinMOSI = 19,
  // Low while a card is in.
  kSdcardPinDetect = 22,

  // A card starts in SPI mode at no more than 400kHz.
  kSdcardInitHz = 400 * 1000,
  // Afterwards it takes 25MHz. The SPI block runs from the peripheral clock,
  // 48MHz once the system clock is raised, and divides it by an even number,
  // so this comes out at 24MHz.
  kSdcardFastHz = 25 * 1000 * 1000,

  // How long a card may take to leave its idle state, to be ready for a
  // command, and to start sending a block. The specification allows a second
  // for the first, and 250 and 100ms for the others on a standard capacity
  // card.
  kSdcardInitTimeoutUs = 1000 * 1000,
  kSdcardReadyTimeoutUs = 500 * 1000,
  kSdcardReadTimeoutUs = 200 * 1000,

  // What MOSI sends when there is nothing to send, and what MISO reads when
  // the card is not driving it.
  kSdcardIdleByte = 0xFF,

  // The commands this uses. ACMD41 is an application command: CMD55 says the
  // next command is one.
  kCommandGoIdle = 0,
  kCommandSendIfCond = 8,
  kCommandSendCSD = 9,
  kCommandStopTransmission = 12,
  kCommandSetBlockLength = 16,
  kCommandReadBlock = 17,
  kCommandReadMultipleBlock = 18,
  kCommandWriteBlock = 24,
  kCommandWriteMultipleBlock = 25,
  kCommandApplication = 55,
  kCommandReadOCR = 58,
  kApplicationSendOpCond = 41,

  // A command frame: the command with its start bits, a 32-bit argument, and
  // a CRC, which SPI mode checks only on CMD0 and CMD8.
  kCommandStartBits = 0x40,
  kCommandFrameBytes = 6,
  kCRCGoIdle = 0x95,
  kCRCSendIfCond = 0x87,
  kCRCUnchecked = 0x01,
  // How many bytes the card may take to answer a command.
  kResponseMaxWaitBytes = 10,

  // R1 responses.
  kR1Ready = 0x00,
  kR1Idle = 0x01,
  kR1IllegalCommand = 0x04,
  kR1NotYet = 0x80,

  // CMD8 asks whether the card takes the host's voltage, and to echo a check
  // pattern. A card that does is version 2.0 or later.
  kSendIfCondVoltage27To36 = 0x1AA,
  kSendIfCondEcho = 0xAA,
  // ACMD41's argument: whether the host takes high capacity cards.
  kOpCondHighCapacity = 1u << 30,
  // OCR bit 30, in its first byte: whether the card is high capacity, and so
  // addressed by block rather than by byte.
  kOCRHighCapacityByte0 = 0x40,

  // The card specific data register, which CMD9 reads as a 16-byte block. Its
  // first two bits say which layout the rest has.
  kCSDBytes = 16,
  kCSDVersionShift = 6,
  kCSDVersion1 = 0,
  kCSDVersion2 = 1,
  // A version 2.0 card's capacity is C_SIZE + 1 times 512KB, a thousand and
  // twenty-four blocks.
  kCSD2BlocksPerUnit = 1024,

  // The tokens around a block: the one before a block read, or written alone,
  // and the one before each block of a multiple block write and after its
  // last.
  kTokenStartBlock = 0xFE,
  kTokenStartMultipleWrite = 0xFC,
  kTokenStopMultipleWrite = 0xFD,
  kDataResponseMask = 0x1F,
  kDataResponseAccepted = 0x05,
  kBlockCRCBytes = 2,
};

static bool g_is_byte_addressed;
static uint32_t g_num_blocks;
static uint32_t g_clock_hz;

static uint8_t Transfer(uint8_t out) {
  uint8_t in = kSdcardIdleByte;
  spi_write_read_blocking(kSdcardSPI, &out, &in, 1);
  return in;
}

static bool WaitReady(void) {
  const uint64_t deadline = time_us_64() + kSdcardReadyTimeoutUs;
  while (Transfer(kSdcardIdleByte) != kSdcardIdleByte) {
    if (time_us_64() > deadline) {
      return false;
    }
  }
  return true;
}

// A byte after CS goes high is what makes the card let go of MISO.
static void Deselect(void) {
  gpio_put(kSdcardPinCS, 1);
  Transfer(kSdcardIdleByte);
}

static bool Select(void) {
  gpio_put(kSdcardPinCS, 0);
  Transfer(kSdcardIdleByte);
  if (WaitReady()) {
    return true;
  }
  Deselect();
  return false;
}

// Send a command and return its R1 response, leaving the card selected for
// whatever follows the response. Each command starts from a fresh select,
// which waits for the card to be ready for it.
static void SendFrame(uint8_t command, uint32_t argument) {
  uint8_t crc = kCRCUnchecked;
  if (command == kCommandGoIdle) {
    crc = kCRCGoIdle;
  } else if (command == kCommandSendIfCond) {
    crc = kCRCSendIfCond;
  }
  const uint8_t frame[kCommandFrameBytes] = {
      (uint8_t)(kCommandStartBits | command),
      (uint8_t)(argument >> 24),
      (uint8_t)(argument >> 16),
      (uint8_t)(argument >> 8),
      (uint8_t)argument,
      crc,
  };
  spi_write_blocking(kSdcardSPI, frame, kCommandFrameBytes);
}

static uint8_t ReadR1(void) {
  uint8_t response = kR1NotYet;
  for (uint8_t i = 0; i < kResponseMaxWaitBytes; ++i) {
    response = Transfer(kSdcardIdleByte);
    if ((response & kR1NotYet) == 0) {
      break;
    }
  }
  return response;
}

static uint8_t SendCommand(uint8_t command, uint32_t argument) {
  Deselect();
  if (!Select()) {
    return kR1NotYet;
  }
  SendFrame(command, argument);
  return ReadR1();
}

static uint8_t SendApplicationCommand(uint8_t command, uint32_t argument) {
  const uint8_t response = SendCommand(kCommandApplication, 0);
  if (response > kR1Idle) {
    return response;
  }
  return SendCommand(command, argument);
}

// Wait for the card to leave its idle state, which ACMD41 starts and asks
// after.
static bool WaitForOpCond(uint32_t argument) {
  const uint64_t deadline = time_us_64() + kSdcardInitTimeoutUs;
  for (;;) {
    const uint8_t response =
        SendApplicationCommand(kApplicationSendOpCond, argument);
    if (response == kR1Ready) {
      return true;
    }
    if (response != kR1Idle || time_us_64() > deadline) {
      return false;
    }
  }
}

// The sequence in section 7.2.1 of the specification, less MMC.
static bool ReadData(uint8_t* data, uint32_t length);

// The card's size in blocks, from its card specific data, or 0 if it could not
// be read or has a layout this does not know.
static uint32_t ReadNumBlocks(void) {
  uint8_t csd[kCSDBytes];
  if (SendCommand(kCommandSendCSD, 0) != kR1Ready ||
      !ReadData(csd, sizeof(csd))) {
    return 0;
  }
  const uint8_t version = csd[0] >> kCSDVersionShift;
  if (version == kCSDVersion2) {
    // C_SIZE is bits 69 to 48.
    const uint32_t c_size =
        ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
    return (c_size + 1) * kCSD2BlocksPerUnit;
  }
  if (version == kCSDVersion1) {
    // (C_SIZE + 1) << (C_SIZE_MULT + 2) blocks of 2^READ_BL_LEN bytes:
    // READ_BL_LEN is bits 83 to 80, C_SIZE 73 to 62, C_SIZE_MULT 49 to 47.
    const uint8_t read_bl_len = csd[5] & 0x0F;
    const uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                            ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
    const uint8_t c_size_mult =
        (uint8_t)(((csd[9] & 0x03) << 1) | (csd[10] >> 7));
    const uint8_t shift = (uint8_t)(c_size_mult + 2 + read_bl_len);
    // In 512-byte blocks: at least 2^9 bytes each.
    return shift >= 9 ? (c_size + 1) << (shift - 9) : 0;
  }
  return 0;
}

static bool BringUp(void) {
  // At least 74 clocks with CS high, for the card to finish powering up.
  gpio_put(kSdcardPinCS, 1);
  for (uint8_t i = 0; i < 10; ++i) {
    Transfer(kSdcardIdleByte);
  }

  // CMD0 with CS low is what puts the card in SPI mode. Its R1 can be lost to
  // whatever the card was doing, so it is worth more than one try.
  uint8_t response = kR1NotYet;
  for (uint8_t i = 0; i < 4 && response != kR1Idle; ++i) {
    response = SendCommand(kCommandGoIdle, 0);
  }
  if (response != kR1Idle) {
    return false;
  }

  response = SendCommand(kCommandSendIfCond, kSendIfCondVoltage27To36);
  if (response == kR1Idle) {
    // Version 2.0 or later: the R7 response's last two bytes echo the
    // voltage and the check pattern.
    uint8_t r7[4];
    for (uint8_t i = 0; i < sizeof(r7); ++i) {
      r7[i] = Transfer(kSdcardIdleByte);
    }
    if (r7[3] != kSendIfCondEcho) {
      return false;
    }
    if (!WaitForOpCond(kOpCondHighCapacity)) {
      return false;
    }
    if (SendCommand(kCommandReadOCR, 0) != kR1Ready) {
      return false;
    }
    uint8_t ocr[4];
    for (uint8_t i = 0; i < sizeof(ocr); ++i) {
      ocr[i] = Transfer(kSdcardIdleByte);
    }
    g_is_byte_addressed = (ocr[0] & kOCRHighCapacityByte0) == 0;
  } else if (response & kR1IllegalCommand) {
    // Version 1: standard capacity, addressed by byte.
    if (!WaitForOpCond(0)) {
      return false;
    }
    g_is_byte_addressed = true;
  } else {
    return false;
  }

  // A byte addressed card's block length can differ; a block addressed
  // card's is always 512.
  if (g_is_byte_addressed &&
      SendCommand(kCommandSetBlockLength, kSdcardBlockSize) != kR1Ready) {
    return false;
  }
  g_num_blocks = ReadNumBlocks();
  return true;
}

bool SdcardIsInserted(void) {
  gpio_init(kSdcardPinDetect);
  gpio_set_dir(kSdcardPinDetect, GPIO_IN);
  gpio_pull_up(kSdcardPinDetect);
  // Give the pull-up a moment to bring an empty slot's line high.
  busy_wait_us(10);
  return !gpio_get(kSdcardPinDetect);
}

bool SdcardInit(void) {
  g_num_blocks = 0;
  spi_init(kSdcardSPI, kSdcardInitHz);
  gpio_set_function(kSdcardPinMISO, GPIO_FUNC_SPI);
  gpio_set_function(kSdcardPinSCK, GPIO_FUNC_SPI);
  gpio_set_function(kSdcardPinMOSI, GPIO_FUNC_SPI);
  // The card drives MISO open drain until it is in SPI mode.
  gpio_pull_up(kSdcardPinMISO);
  gpio_init(kSdcardPinCS);
  gpio_set_dir(kSdcardPinCS, GPIO_OUT);
  gpio_put(kSdcardPinCS, 1);

  const bool is_up = BringUp();
  Deselect();
  if (is_up) {
    g_clock_hz = spi_set_baudrate(kSdcardSPI, kSdcardFastHz);
  }
  return is_up;
}

uint32_t SdcardGetClockHz(void) { return g_clock_hz; }

uint32_t SdcardGetNumBlocks(void) { return g_num_blocks; }

static uint32_t Address(uint32_t block) {
  return g_is_byte_addressed ? block * kSdcardBlockSize : block;
}

// Read the data block a command's R1 is followed by: its start token, length
// bytes, and the CRC, which is not checked.
static bool ReadData(uint8_t* data, uint32_t length) {
  const uint64_t deadline = time_us_64() + kSdcardReadTimeoutUs;
  uint8_t token;
  while ((token = Transfer(kSdcardIdleByte)) == kSdcardIdleByte) {
    if (time_us_64() > deadline) {
      return false;
    }
  }
  if (token != kTokenStartBlock) {
    return false;
  }
  spi_read_blocking(kSdcardSPI, kSdcardIdleByte, data, length);
  for (uint8_t i = 0; i < kBlockCRCBytes; ++i) {
    Transfer(kSdcardIdleByte);
  }
  return true;
}

// End a multiple block read. The card is sending the next block meanwhile, so
// there is no waiting for it to be ready first: the byte after the command is
// a stuff byte, then comes the R1, then the card is busy until it has
// stopped.
static bool StopReading(void) {
  SendFrame(kCommandStopTransmission, 0);
  Transfer(kSdcardIdleByte);
  const uint8_t response = ReadR1();
  return WaitReady() && response == kR1Ready;
}

// Send a block, after the token that starts it, and wait for the card to have
// programmed it.
static bool WriteData(uint8_t token, const uint8_t* data) {
  Transfer(token);
  spi_write_blocking(kSdcardSPI, data, kSdcardBlockSize);
  for (uint8_t i = 0; i < kBlockCRCBytes; ++i) {
    Transfer(kSdcardIdleByte);
  }
  const uint8_t response = Transfer(kSdcardIdleByte);
  if ((response & kDataResponseMask) != kDataResponseAccepted) {
    return false;
  }
  // The card holds MISO low while it programs the block.
  return WaitReady();
}

// A block alone is CMD17 or CMD24, and a run of them one CMD18 or CMD25,
// which saves a command and a wait for each block after the first: a card
// answering a read starts sending sooner, and programs a run of writes
// together.
bool SdcardReadBlocks(uint32_t block, uint8_t* data, uint32_t num_blocks) {
  bool is_ok;
  if (num_blocks == 1) {
    is_ok = SendCommand(kCommandReadBlock, Address(block)) == kR1Ready &&
            ReadData(data, kSdcardBlockSize);
  } else {
    is_ok = SendCommand(kCommandReadMultipleBlock, Address(block)) == kR1Ready;
    for (uint32_t i = 0; i < num_blocks && is_ok; ++i) {
      is_ok = ReadData(data + i * kSdcardBlockSize, kSdcardBlockSize);
    }
    // A read that failed part way is stopped all the same.
    is_ok = StopReading() && is_ok;
  }
  Deselect();
  return is_ok;
}

bool SdcardWriteBlocks(
    uint32_t block, const uint8_t* data, uint32_t num_blocks) {
  bool is_ok;
  if (num_blocks == 1) {
    is_ok = SendCommand(kCommandWriteBlock, Address(block)) == kR1Ready;
    if (is_ok) {
      Transfer(kSdcardIdleByte);
      is_ok = WriteData(kTokenStartBlock, data);
    }
  } else {
    is_ok = SendCommand(kCommandWriteMultipleBlock, Address(block)) == kR1Ready;
    if (is_ok) {
      Transfer(kSdcardIdleByte);
      for (uint32_t i = 0; i < num_blocks && is_ok; ++i) {
        is_ok =
            WriteData(kTokenStartMultipleWrite, data + i * kSdcardBlockSize);
      }
      // A write that failed part way is stopped all the same, which ends it
      // after the blocks the card accepted.
      Transfer(kTokenStopMultipleWrite);
      Transfer(kSdcardIdleByte);
      is_ok = WaitReady() && is_ok;
    }
  }
  Deselect();
  return is_ok;
}

bool SdcardSync(void) {
  if (!Select()) {
    return false;
  }
  Deselect();
  return true;
}
