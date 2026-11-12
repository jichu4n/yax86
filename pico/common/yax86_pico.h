// What the Raspberry Pi Pico, an RP2040 with 264KB of SRAM, gives the guest.
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
//   - Without it, such as on a bare Pico, kPicoGuestSRAMSize of conventional
//     memory held directly in the same SRAM.
#ifndef YAX86_PICO_H
#define YAX86_PICO_H

enum {
  // The SRAM given to guest memory: conventional memory itself without the
  // PSRAM, and the pager's frames with it. 128KB is what fits alongside
  // video memory, the SDK, the USB stack and the runtime.
  kPicoGuestSRAMSize = 128 * 1024,
};

#endif  // YAX86_PICO_H
