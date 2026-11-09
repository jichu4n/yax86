// The register protocol is ClockworkPi's keyboard firmware's
// (Code/picocalc_keyboard in the PicoCalc repository): reg.h for the register
// numbers, and receiveEvent() in picocalc_keyboard.ino for what each one
// answers.

#include "southbridge.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/error.h"
#include "pico/time.h"

// The I2C block the controller is wired to.
static i2c_inst_t* const kSouthbridgeI2C = i2c1;

enum {
  kSouthbridgePinSDA = 6,
  kSouthbridgePinSCL = 7,
  kSouthbridgeAddress = 0x1F,

  // ClockworkPi's samples run the bus at 10kHz. The controller answers every
  // read at 10, 100 and 400kHz alike, and the key FIFO is read once a batch
  // whatever the rate, so this is standard mode: well within both parts, and
  // a tenth of the time on the bus that 10kHz takes.
  kSouthbridgeI2CHz = 100 * 1000,

  // How long a transfer may take before the controller is taken to be absent.
  // A two byte read at 100kHz is about 0.3ms, so this is generous.
  kSouthbridgeTimeoutUs = 50 * 1000,
  // ClockworkPi's samples wait this long between writing a register number and
  // reading its value. The controller prepares its answer as the register
  // number arrives, and the key FIFO poll below reads straight on without
  // ever finding it unready, so this is caution kept for the one read made at
  // startup.
  kSouthbridgeReplyDelayMs = 16,

  // How long to keep asking a controller that does not acknowledge. From v1.6
  // its firmware restarts its I2C peripheral once the bus has been quiet for
  // 2.5 seconds, so this covers one restart. Earlier firmware has no such
  // recovery, and a retry there only helps if the fault clears by itself.
  kSouthbridgeAnswerWindowMs = 3000,
  kSouthbridgeRetryMs = 250,

  // How long a read of the key FIFO may take before the bus is taken to be
  // stuck and the I2C block is reset. A read is about 50 bit times, 0.5ms at
  // 100kHz.
  kSouthbridgePollTimeoutUs = 50 * 1000,

  kRegisterVersion = 0x01,
  kRegisterKeyFIFO = 0x09,
  // The bytes a register answers with.
  kRegisterAnswerBytes = 2,
};

// The key FIFO read in flight, if any.
static bool g_poll_in_flight;
static uint64_t g_poll_started_us;
// Whether a read has found the key FIFO empty yet.
static bool g_poll_caught_up;
static SouthbridgePollStats g_poll_stats;

void SouthbridgeInit(void) {
  i2c_init(kSouthbridgeI2C, kSouthbridgeI2CHz);
  gpio_set_function(kSouthbridgePinSDA, GPIO_FUNC_I2C);
  gpio_set_function(kSouthbridgePinSCL, GPIO_FUNC_I2C);
  gpio_pull_up(kSouthbridgePinSDA);
  gpio_pull_up(kSouthbridgePinSCL);
}

// The I2C block's target address can only be changed while it is disabled,
// which the SDK's own transfers do every time. The poll below sets it once
// and leaves the block running.
static void SetTargetAddress(void) {
  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  hw->enable = 0;
  hw->tar = kSouthbridgeAddress;
  hw->enable = 1;
}

// Read a register's two byte answer.
static bool SouthbridgeReadRegister(uint8_t reg, uint8_t answer[2]) {
  if (i2c_write_timeout_us(
          kSouthbridgeI2C, kSouthbridgeAddress, &reg, 1, false,
          kSouthbridgeTimeoutUs) != 1) {
    return false;
  }
  sleep_ms(kSouthbridgeReplyDelayMs);
  return i2c_read_timeout_us(
             kSouthbridgeI2C, kSouthbridgeAddress, answer, 2, false,
             kSouthbridgeTimeoutUs) == 2;
}

// Read a register, retrying for a while if the controller does not answer.
//
// On one unit the controller acknowledged nothing for two boots in a row and
// then answered every read on every boot after, including straight after the
// factory firmware had been polling it. What caused that is not known, which
// is why this retries rather than giving up on the first try.
static bool SouthbridgeReadRegisterPatiently(uint8_t reg, uint8_t answer[2]) {
  const absolute_time_t deadline =
      make_timeout_time_ms(kSouthbridgeAnswerWindowMs);
  while (!SouthbridgeReadRegister(reg, answer)) {
    if (time_reached(deadline)) {
      return false;
    }
    sleep_ms(kSouthbridgeRetryMs);
  }
  return true;
}

bool SouthbridgeReadVersion(uint8_t* version) {
  // The first byte is always zero; the version is the second.
  uint8_t answer[2] = {0, 0};
  if (!SouthbridgeReadRegisterPatiently(kRegisterVersion, answer)) {
    return false;
  }
  *version = answer[1];
  return true;
}

// Queue a whole read of the key FIFO: the register number, then a two byte
// read, each ending in a STOP. The I2C block carries the commands out from its
// own FIFO, so nothing waits for the bus. The controller prepares its answer
// as the register number arrives, so the read can follow straight on.
static void StartKeyFIFORead(void) {
  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  hw->data_cmd = I2C_IC_DATA_CMD_STOP_BITS | kRegisterKeyFIFO;
  hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS;
  hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS | I2C_IC_DATA_CMD_STOP_BITS;
  g_poll_started_us = time_us_64();
  g_poll_in_flight = true;
}

static void DrainReceiveFIFO(void) {
  while (i2c_get_read_available(kSouthbridgeI2C) > 0) {
    i2c_read_byte_raw(kSouthbridgeI2C);
  }
}

bool SouthbridgePollKey(SouthbridgeKeyEvent* event) {
  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  if (!g_poll_in_flight) {
    SetTargetAddress();
    StartKeyFIFORead();
    return false;
  }

  bool has_key = false;
  if (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) {
    // The block has flushed what was left of the read and sent a STOP.
    // Reading the register clears the abort.
    (void)hw->clr_tx_abrt;
    DrainReceiveFIFO();
    ++g_poll_stats.aborts;
  } else if (i2c_get_read_available(kSouthbridgeI2C) >= kRegisterAnswerBytes) {
    const uint8_t state = i2c_read_byte_raw(kSouthbridgeI2C);
    const uint8_t key = i2c_read_byte_raw(kSouthbridgeI2C);
    ++g_poll_stats.reads;
    if (state == kSouthbridgeKeyIdle) {
      g_poll_caught_up = true;
    } else if (!g_poll_caught_up) {
      ++g_poll_stats.stale_keys;
    } else if (state <= kSouthbridgeKeyReleased) {
      event->state = (SouthbridgeKeyState)state;
      event->key = key;
      ++g_poll_stats.keys;
      has_key = true;
    }
  } else if (time_us_64() - g_poll_started_us > kSouthbridgePollTimeoutUs) {
    // Start again from a freshly reset block, which drops whatever it was
    // stuck on.
    i2c_deinit(kSouthbridgeI2C);
    SouthbridgeInit();
    SetTargetAddress();
    ++g_poll_stats.timeouts;
  } else {
    return false;
  }
  StartKeyFIFORead();
  return has_key;
}

SouthbridgePollStats SouthbridgeGetPollStats(void) { return g_poll_stats; }
