// What the Raspberry Pi Pico 2, an RP2350 with 520KB of SRAM, gives the guest.
// pico/bench and pico/picocalc both build against this, so the bench
// measures the machine the runtime runs.
//
// A guest has one of two memory layouts, chosen at startup by whether the
// PSRAM answers (psram.h):
//
//   - With the PSRAM, the most conventional memory there is,
//     Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE, all of it paged (pager.h):
//     the pages live in PSRAM and are brought into kPicoGuestSRAMSize of
//     frames in SRAM when the guest touches them.
//   - Without it, such as on a bare Pico 2, kPicoGuestSRAMSize of conventional
//     memory held directly in the same SRAM.
#ifndef YAX86_PICO_H
#define YAX86_PICO_H

enum {
  // The SRAM given to guest memory: conventional memory itself without the
  // PSRAM, and the pager's frames with it. 384KB is a multiple of 16KB for
  // GLaBIOS, chosen by the PicoCalc workloads ("Paging guest RAM" in
  // pico/picocalc/AGENTS.md), and leaves the PicoCalc runtime about 10KB of
  // the 512KB main SRAM bank.
  kPicoGuestSRAMSize = 384 * 1024,
};

// GLaBIOS steps through conventional memory in 16KB blocks and probes only up
// to offset 0x2000 in each, so a partial block past 8KB makes it report
// unmapped memory as present in the direct layout.
_Static_assert(
    kPicoGuestSRAMSize % (16 * 1024) == 0,
    "GLaBIOS sizes memory in 16KB blocks");

#endif  // YAX86_PICO_H
