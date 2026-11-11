// The PicoCalc's 8MB of PSRAM, an ESP-PSRAM64H on one-bit SPI through PIO,
// on the mainboard the Pico plugs into. On a bare Pico its pins are not
// connected, and PsramInit() fails.
//
// Every wait is bounded, so a chip that stops answering costs a timeout and
// an error, never a hang. Called from core 0 only.
#ifndef YAX86_PICO_PSRAM_H
#define YAX86_PICO_PSRAM_H

#include <stdbool.h>
#include <stdint.h>

enum {
  kPsramSize = 8 * 1024 * 1024,
  // What a read or write moves: a whole number of words.
  kPsramAlignment = 4,
};

typedef enum PsramState {
  // Not brought up.
  kPsramOff,
  // Answered and kept what was written to it.
  kPsramOk,
  // The system clock does not divide to the serial clock.
  kPsramUnsupportedClock,
  // The ID read back was not the chip's.
  kPsramNoChip,
  // The self-test read back something other than it wrote.
  kPsramTestFailed,
  // A transfer timed out.
  kPsramTimedOut,
  // The last start hung bringing it up, so this one did not try.
  kPsramSkippedAfterHang,
} PsramState;

typedef struct PsramStats {
  PsramState state;
  uint32_t serial_hz;
  // The chip's known-good-die byte, 0x5D for a good one.
  uint8_t kgd;
  // What the self-test took to read and write a 512-byte block.
  uint32_t read_512_us;
  uint32_t write_512_us;
  // Bytes moved since start, and transfers that timed out.
  uint64_t bytes_read;
  uint64_t bytes_written;
  uint32_t timeouts;
} PsramStats;

// Bring the chip up and test it, returning whether it is usable. skip says
// the last start hung doing so, and leaves it off.
bool PsramInit(bool skip);

// Read or write size bytes at address, both multiples of kPsramAlignment,
// to or from a buffer aligned to it. Returns false on a timeout, or if the
// chip is not usable, leaving data unspecified.
bool PsramRead(uint32_t address, void* data, uint32_t size);
bool PsramWrite(uint32_t address, const void* data, uint32_t size);

PsramStats PsramGetStats(void);
const char* PsramStateName(PsramState state);

#endif  // YAX86_PICO_PSRAM_H
