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

  // ClockworkPi's samples run the bus at 10kHz. Whether it takes more is a
  // question for when the keyboard is read on every frame.
  kSouthbridgeI2CHz = 10 * 1000,

  // How long a transfer may take before the controller is taken to be absent.
  // A two byte read at 10kHz is about 3ms, so this is generous.
  kSouthbridgeTimeoutUs = 50 * 1000,
  // ClockworkPi's samples wait this long between writing a register number and
  // reading its value. The controller's main loop runs every 10ms, and the
  // answer is prepared when the register number arrives.
  kSouthbridgeReplyDelayMs = 16,

  // How long to keep asking a controller that does not acknowledge. From v1.6
  // its firmware restarts its I2C peripheral once the bus has been quiet for
  // 2.5 seconds, so this covers one restart. Earlier firmware has no such
  // recovery, and a retry there only helps if the fault clears by itself.
  kSouthbridgeAnswerWindowMs = 3000,
  kSouthbridgeRetryMs = 250,

  kRegisterVersion = 0x01,
};

void SouthbridgeInit(void) {
  i2c_init(kSouthbridgeI2C, kSouthbridgeI2CHz);
  gpio_set_function(kSouthbridgePinSDA, GPIO_FUNC_I2C);
  gpio_set_function(kSouthbridgePinSCL, GPIO_FUNC_I2C);
  gpio_pull_up(kSouthbridgePinSDA);
  gpio_pull_up(kSouthbridgePinSCL);
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
