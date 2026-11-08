// The PicoCalc's STM32 keyboard and power controller, which ClockworkPi's
// firmware and the PicoCalc community call the south bridge.
//
// It sits on I2C1 at address 0x1F and is read a register at a time: write the
// register number, then read two bytes back.
#ifndef YAX86_PICOCALC_SOUTHBRIDGE_H
#define YAX86_PICOCALC_SOUTHBRIDGE_H

#include <stdbool.h>
#include <stdint.h>

void SouthbridgeInit(void);

// The controller's firmware version, as ClockworkPi encodes it: the major
// version in the high nibble and the minor in the low, so 0x16 is v1.6.
//
// The register only answers from v1.6 on. Earlier firmware answers any
// register it does not know with zeroes, so a version of 0 means older than
// v1.6. Returns false if the controller did not answer at all.
bool SouthbridgeReadVersion(uint8_t* version);

#endif  // YAX86_PICOCALC_SOUTHBRIDGE_H
