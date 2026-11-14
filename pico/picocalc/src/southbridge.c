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

  // ClockworkPi's samples and its PicoMite build run the bus at 10kHz, and
  // their headers say the keyboard's should be 10kHz, without saying why. The
  // controller answers every read at 10, 100 and 400kHz alike, but the key
  // FIFO is read once a batch whatever the rate, and a read at 10kHz is done
  // well within one, so a faster bus would buy nothing.
  kSouthbridgeI2CHz = 10 * 1000,

  // How long a transfer may take before the controller is taken to be absent.
  // A two byte read at 10kHz is about 3ms, so this is generous.
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
  // stuck and the I2C block is reset. A read is about 50 bit times, 5ms at
  // 10kHz.
  kSouthbridgePollTimeoutUs = 50 * 1000,

  kRegisterVersion = 0x01,
  kRegisterDisplayBacklight = 0x05,
  kRegisterKeyFIFO = 0x09,
  kRegisterKeyboardBacklight = 0x0A,
  // Answers the register number, then the percentage in the low seven bits,
  // with the top bit set while charging.
  kRegisterBattery = 0x0B,
  // Setting the top bit of a register number writes the byte that follows it.
  kRegisterWriteBit = 0x80,
  // The earliest firmware version where reading and writing both backlight
  // registers is supported without side effects.
  kMinBacklightVersion = 0x16,
  kNumBacklightRegisters = 2,
  kBatteryChargingBit = 0x80,
  kBatteryPercentMask = 0x7F,
  kBatteryMaxPercent = 100,
  // How often the poll reads the battery in place of the key FIFO. The
  // controller measures it every 20 seconds, so more often would find
  // nothing new.
  kBatteryReadIntervalUs = 10 * 1000 * 1000,
  // How often the poll reads the backlights while they are on, besides reading
  // them straight after Alt comes up.
  kBacklightReadIntervalUs = 5 * 1000 * 1000,
  // The bytes a register answers with.
  kRegisterAnswerBytes = 2,
};

// Whether the controller's firmware supports the backlight registers.
static bool g_has_backlights;
// Whether the poll keeps g_backlights fresh from the controller, and how many
// of the two registers are queued to be read next.
static bool g_should_poll_backlights;
static uint8_t g_num_pending_backlight_reads;
static uint64_t g_next_backlight_read_us;
static SouthbridgeBacklights g_backlights;
// The read in flight, if any, and which register it is of.
static bool g_is_poll_in_flight;
static uint8_t g_poll_register;
// A key collected while waiting out an in-flight read before a synchronous
// transfer, to be returned by the next SouthbridgePollKey().
static bool g_has_saved_key;
static SouthbridgeKeyEvent g_saved_key;
// When the poll next reads the battery rather than the key FIFO: straight
// away, the first time.
static uint64_t g_next_battery_read_us;
static SouthbridgeBattery g_battery;
static uint64_t g_poll_started_us;
// Whether a read has found the key FIFO empty yet.
static bool g_has_poll_caught_up;
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

// Write a one byte value to a register.
static bool SouthbridgeWriteRegister(uint8_t reg, uint8_t value) {
  const uint8_t command[2] = {(uint8_t)(reg | kRegisterWriteBit), value};
  return i2c_write_timeout_us(
             kSouthbridgeI2C, kSouthbridgeAddress, command, sizeof(command),
             false, kSouthbridgeTimeoutUs) == sizeof(command);
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
    g_has_backlights = false;
    return false;
  }
  *version = answer[1];
  g_has_backlights = *version >= kMinBacklightVersion;
  return true;
}

bool SouthbridgeHasBacklights(void) { return g_has_backlights; }

// Queue a whole read of the key FIFO, or every so often of the battery or a
// backlight: the register number, then a two byte read, each ending in a STOP.
// The I2C block carries the commands out from its own FIFO, so nothing waits
// for the bus. The controller prepares its answer as the register number
// arrives, so the read can follow straight on.
static void StartKeyFIFORead(void) {
  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  const uint64_t now_us = time_us_64();
  if (g_should_poll_backlights && g_num_pending_backlight_reads == 0 &&
      now_us >= g_next_backlight_read_us) {
    g_num_pending_backlight_reads = kNumBacklightRegisters;
    g_next_backlight_read_us = now_us + kBacklightReadIntervalUs;
  }
  g_poll_register = kRegisterKeyFIFO;
  if (now_us >= g_next_battery_read_us) {
    g_poll_register = kRegisterBattery;
    g_next_battery_read_us = now_us + kBatteryReadIntervalUs;
  } else if (g_should_poll_backlights && g_num_pending_backlight_reads > 0) {
    g_poll_register = g_num_pending_backlight_reads == kNumBacklightRegisters
                          ? kRegisterDisplayBacklight
                          : kRegisterKeyboardBacklight;
    --g_num_pending_backlight_reads;
  }
  hw->data_cmd = I2C_IC_DATA_CMD_STOP_BITS | g_poll_register;
  hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS;
  hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS | I2C_IC_DATA_CMD_STOP_BITS;
  g_poll_started_us = time_us_64();
  g_is_poll_in_flight = true;
}

static void DrainReceiveFIFO(void) {
  while (i2c_get_read_available(kSouthbridgeI2C) > 0) {
    i2c_read_byte_raw(kSouthbridgeI2C);
  }
}

// Take the two bytes of a completed poll read, returning whether they held a
// key event for the caller.
static bool CollectPollReply(SouthbridgeKeyEvent* event) {
  const uint8_t first = i2c_read_byte_raw(kSouthbridgeI2C);
  const uint8_t second = i2c_read_byte_raw(kSouthbridgeI2C);
  ++g_poll_stats.num_reads;
  if (g_poll_register == kRegisterBattery) {
    const uint8_t percent = second & kBatteryPercentMask;
    g_battery.is_known = percent > 0 && percent <= kBatteryMaxPercent;
    g_battery.percent = percent;
    g_battery.is_charging = (second & kBatteryChargingBit) != 0;
    return false;
  }
  if (g_poll_register == kRegisterDisplayBacklight) {
    if (first == kRegisterDisplayBacklight && g_should_poll_backlights) {
      g_backlights.display = second;
      g_backlights.is_known = true;
    }
    return false;
  }
  if (g_poll_register == kRegisterKeyboardBacklight) {
    if (first == kRegisterKeyboardBacklight && g_should_poll_backlights) {
      g_backlights.keyboard = second;
      g_backlights.is_known = true;
    }
    return false;
  }
  if (first == kSouthbridgeKeyIdle) {
    g_has_poll_caught_up = true;
    return false;
  }
  if (!g_has_poll_caught_up) {
    ++g_poll_stats.num_stale_keys;
    return false;
  }
  if (first <= kSouthbridgeKeyReleased) {
    event->state = (SouthbridgeKeyState)first;
    event->key = second;
    ++g_poll_stats.num_keys;
    if (g_should_poll_backlights && event->state == kSouthbridgeKeyReleased &&
        event->key == kKeyCodeAlt) {
      // Alt with comma, full stop or space changes a backlight on the
      // controller without reporting the second key, so read both once Alt
      // comes up.
      g_num_pending_backlight_reads = kNumBacklightRegisters;
    }
    return true;
  }
  return false;
}

// Wait out a poll read already on the bus before a synchronous transfer,
// keeping any key it brings back for the next SouthbridgePollKey().
static void FinishPollInFlight(void) {
  if (!g_is_poll_in_flight) {
    return;
  }
  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  for (;;) {
    if (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) {
      (void)hw->clr_tx_abrt;
      DrainReceiveFIFO();
      ++g_poll_stats.num_aborts;
      break;
    }
    if (i2c_get_read_available(kSouthbridgeI2C) >= kRegisterAnswerBytes) {
      SouthbridgeKeyEvent key;
      if (!g_has_saved_key && CollectPollReply(&key)) {
        g_saved_key = key;
        g_has_saved_key = true;
      }
      break;
    }
    if (time_us_64() - g_poll_started_us > kSouthbridgePollTimeoutUs) {
      i2c_deinit(kSouthbridgeI2C);
      SouthbridgeInit();
      ++g_poll_stats.num_timeouts;
      break;
    }
    tight_loop_contents();
  }
  // Wait for the STOP after the last byte before the SDK disables the block to
  // set its target address.
  const uint64_t stop_start_us = time_us_64();
  while ((hw->status & I2C_IC_STATUS_ACTIVITY_BITS) != 0 &&
         time_us_64() - stop_start_us < kSouthbridgePollTimeoutUs) {
    tight_loop_contents();
  }
  g_is_poll_in_flight = false;
}

bool SouthbridgeReadBacklights(uint8_t* display, uint8_t* keyboard) {
  if (!g_has_backlights) {
    return false;
  }
  FinishPollInFlight();
  uint8_t display_answer[2] = {0, 0};
  uint8_t keyboard_answer[2] = {0, 0};
  if (!SouthbridgeReadRegister(kRegisterDisplayBacklight, display_answer) ||
      display_answer[0] != kRegisterDisplayBacklight ||
      !SouthbridgeReadRegister(kRegisterKeyboardBacklight, keyboard_answer) ||
      keyboard_answer[0] != kRegisterKeyboardBacklight) {
    return false;
  }
  *display = display_answer[1];
  *keyboard = keyboard_answer[1];
  g_backlights = (SouthbridgeBacklights){
      .is_known = true,
      .display = *display,
      .keyboard = *keyboard,
  };
  return true;
}

bool SouthbridgeSetBacklights(uint8_t display, uint8_t keyboard) {
  if (!g_has_backlights) {
    return false;
  }
  FinishPollInFlight();
  g_should_poll_backlights = false;
  g_num_pending_backlight_reads = 0;
  if (!SouthbridgeWriteRegister(kRegisterDisplayBacklight, display) ||
      !SouthbridgeWriteRegister(kRegisterKeyboardBacklight, keyboard)) {
    return false;
  }
  g_backlights = (SouthbridgeBacklights){
      .is_known = true,
      .display = display,
      .keyboard = keyboard,
  };
  if (display > 0) {
    g_should_poll_backlights = true;
    g_next_backlight_read_us = time_us_64() + kBacklightReadIntervalUs;
  }
  return true;
}

SouthbridgeBacklights SouthbridgeGetBacklights(void) { return g_backlights; }

bool SouthbridgePollKey(SouthbridgeKeyEvent* event) {
  if (g_has_saved_key) {
    *event = g_saved_key;
    g_has_saved_key = false;
    if (!g_is_poll_in_flight) {
      SetTargetAddress();
      StartKeyFIFORead();
    }
    return true;
  }

  i2c_hw_t* const hw = i2c_get_hw(kSouthbridgeI2C);
  if (!g_is_poll_in_flight) {
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
    ++g_poll_stats.num_aborts;
  } else if (i2c_get_read_available(kSouthbridgeI2C) >= kRegisterAnswerBytes) {
    has_key = CollectPollReply(event);
  } else if (time_us_64() - g_poll_started_us > kSouthbridgePollTimeoutUs) {
    // Start again from a freshly reset block, which drops whatever it was
    // stuck on.
    i2c_deinit(kSouthbridgeI2C);
    SouthbridgeInit();
    SetTargetAddress();
    ++g_poll_stats.num_timeouts;
  } else {
    return false;
  }
  StartKeyFIFORead();
  return has_key;
}

SouthbridgePollStats SouthbridgeGetPollStats(void) { return g_poll_stats; }

SouthbridgeBattery SouthbridgeGetBattery(void) { return g_battery; }
