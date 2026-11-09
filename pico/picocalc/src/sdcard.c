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
  kCommandSetBlockLength = 16,
  kCommandReadBlock = 17,
  kCommandWriteBlock = 24,
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

  // The tokens around a block.
  kTokenStartBlock = 0xFE,
  kDataResponseMask = 0x1F,
  kDataResponseAccepted = 0x05,
  kBlockCRCBytes = 2,
};

static bool g_byte_addressed;
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
static uint8_t SendCommand(uint8_t command, uint32_t argument) {
  Deselect();
  if (!Select()) {
    return kR1NotYet;
  }
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
  uint8_t response = kR1NotYet;
  for (uint8_t i = 0; i < kResponseMaxWaitBytes; ++i) {
    response = Transfer(kSdcardIdleByte);
    if ((response & kR1NotYet) == 0) {
      break;
    }
  }
  return response;
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
    g_byte_addressed = (ocr[0] & kOCRHighCapacityByte0) == 0;
  } else if (response & kR1IllegalCommand) {
    // Version 1: standard capacity, addressed by byte.
    if (!WaitForOpCond(0)) {
      return false;
    }
    g_byte_addressed = true;
  } else {
    return false;
  }

  // A byte addressed card's block length can differ; a block addressed
  // card's is always 512.
  if (g_byte_addressed &&
      SendCommand(kCommandSetBlockLength, kSdcardBlockSize) != kR1Ready) {
    return false;
  }
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
  spi_init(kSdcardSPI, kSdcardInitHz);
  gpio_set_function(kSdcardPinMISO, GPIO_FUNC_SPI);
  gpio_set_function(kSdcardPinSCK, GPIO_FUNC_SPI);
  gpio_set_function(kSdcardPinMOSI, GPIO_FUNC_SPI);
  // The card drives MISO open drain until it is in SPI mode.
  gpio_pull_up(kSdcardPinMISO);
  gpio_init(kSdcardPinCS);
  gpio_set_dir(kSdcardPinCS, GPIO_OUT);
  gpio_put(kSdcardPinCS, 1);

  const bool up = BringUp();
  Deselect();
  if (up) {
    g_clock_hz = spi_set_baudrate(kSdcardSPI, kSdcardFastHz);
  }
  return up;
}

uint32_t SdcardGetClockHz(void) { return g_clock_hz; }

static uint32_t Address(uint32_t block) {
  return g_byte_addressed ? block * kSdcardBlockSize : block;
}

static bool ReadBlock(uint32_t block, uint8_t* data) {
  if (SendCommand(kCommandReadBlock, Address(block)) != kR1Ready) {
    return false;
  }
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
  spi_read_blocking(kSdcardSPI, kSdcardIdleByte, data, kSdcardBlockSize);
  for (uint8_t i = 0; i < kBlockCRCBytes; ++i) {
    Transfer(kSdcardIdleByte);
  }
  return true;
}

static bool WriteBlock(uint32_t block, const uint8_t* data) {
  if (SendCommand(kCommandWriteBlock, Address(block)) != kR1Ready) {
    return false;
  }
  Transfer(kSdcardIdleByte);
  Transfer(kTokenStartBlock);
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

bool SdcardReadBlocks(uint32_t block, uint8_t* data, uint32_t count) {
  bool ok = true;
  for (uint32_t i = 0; i < count && ok; ++i) {
    ok = ReadBlock(block + i, data + i * kSdcardBlockSize);
  }
  Deselect();
  return ok;
}

bool SdcardWriteBlocks(uint32_t block, const uint8_t* data, uint32_t count) {
  bool ok = true;
  for (uint32_t i = 0; i < count && ok; ++i) {
    ok = WriteBlock(block + i, data + i * kSdcardBlockSize);
  }
  Deselect();
  return ok;
}

bool SdcardSync(void) {
  if (!Select()) {
    return false;
  }
  Deselect();
  return true;
}
