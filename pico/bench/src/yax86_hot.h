// Places the emulator's per-instruction hot path in SRAM.
//
// Force-included into the firmware's translation units so that it applies to
// the core without the core knowing anything about this target. Code otherwise
// runs from QSPI flash through a 16KB XIP cache, which the interpreter does not
// fit inside.
#ifndef YAX86_PICO_HOT_H
#define YAX86_PICO_HOT_H

#include "pico.h"

#define YAX86_HOT __not_in_flash("yax86_code")
#define YAX86_HOT_DATA __not_in_flash("yax86_data")

#endif  // YAX86_PICO_HOT_H
