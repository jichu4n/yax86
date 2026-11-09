// The floppy in drive A: an image on the SD card if there is one, or else the
// boot floppy built into flash.
//
// The image on the card is `yax86/floppy_a.img`, a 360KB floppy, and what the
// guest writes to it is written to the card. The built-in floppy cannot be
// written, and what the guest writes to it is dropped.
#ifndef YAX86_PICOCALC_FLOPPY_H
#define YAX86_PICOCALC_FLOPPY_H

#include <stdint.h>

#include "core/fdc.h"

// Where drive A's floppy came from, and if it is the built-in one, why.
typedef enum FloppySource {
  kFloppySourceSDCard = 0,
  kFloppySourceNoCard,
  kFloppySourceCardNotAnswering,
  kFloppySourceNoFilesystem,
  kFloppySourceNoImage,
  kFloppySourceWrongSize,
} FloppySource;

// What reading and writing the image has cost, for the report.
typedef struct FloppyStats {
  FloppySource source;
  // Sectors read from and written to the card, reads and writes that failed,
  // and the time they took, which the machine waits for.
  uint32_t sector_reads;
  uint32_t sector_writes;
  uint32_t errors;
  uint64_t read_us;
  uint64_t write_us;
} FloppyStats;

// Look for the image on the SD card, and return where the floppy will come
// from. Call once, before FloppyInsert().
FloppySource FloppyOpen(void);

// Hand the floppy to the floppy controller, in drive A.
void FloppyInsert(YFDCState* fdc);

// A few words on where the floppy came from, for the status line.
const char* FloppyDescribeSource(FloppySource source);

FloppyStats FloppyGetStats(void);

#endif  // YAX86_PICOCALC_FLOPPY_H
