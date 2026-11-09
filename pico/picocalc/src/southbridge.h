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

// What became of a key, as the controller's key FIFO reports it.
typedef enum SouthbridgeKeyState {
  // The FIFO was empty.
  kSouthbridgeKeyIdle = 0,
  kSouthbridgeKeyPressed,
  // Reported every 100ms once a key has been held for 300ms, except for the
  // keys whose repeat the controller reports as presses instead.
  kSouthbridgeKeyHeld,
  kSouthbridgeKeyReleased,
} SouthbridgeKeyState;

// One entry of the key FIFO: a key, in ClockworkPi's key codes - ASCII, with
// the keys ASCII has no code for above 0x80 - and what became of it.
typedef struct SouthbridgeKeyEvent {
  SouthbridgeKeyState state;
  uint8_t key;
} SouthbridgeKeyEvent;

// How the key FIFO poll has gone, for the report.
typedef struct SouthbridgePollStats {
  // Reads that completed, the battery's included, and how many held a key.
  uint32_t reads;
  uint32_t keys;
  // Keys dropped because they were already queued when the poll started.
  uint32_t stale_keys;
  // Reads the controller did not acknowledge, and reads that never completed.
  uint32_t aborts;
  uint32_t timeouts;
} SouthbridgePollStats;

// Move the key FIFO poll on, without ever waiting for the bus. Each call
// collects the read started by the call before it, if it has completed, and
// starts the next, so call it often - once a batch. Returns true, with the
// key in event, if the read it collected held one.
//
// The controller keeps running while the RP2040 restarts, and queues keys
// whether or not anything reads them, so its FIFO can be full of keys pressed
// long before this firmware started. Those are dropped: every key up to the
// first read that finds the FIFO empty.
bool SouthbridgePollKey(SouthbridgeKeyEvent* event);

SouthbridgePollStats SouthbridgeGetPollStats(void);

// The battery, as the controller last reported it. The key FIFO poll reads it
// every few seconds in place of the FIFO, and the controller itself only
// measures it every 20 seconds.
typedef struct SouthbridgeBattery {
  // Whether there is a reading. There is none until the first read, nor
  // while the controller reports no battery - it reports 0% then - nor from
  // firmware that answers the register with something out of range.
  bool known;
  uint8_t percent;
  bool charging;
} SouthbridgeBattery;

SouthbridgeBattery SouthbridgeGetBattery(void);

#endif  // YAX86_PICOCALC_SOUTHBRIDGE_H
