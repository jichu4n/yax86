// The PicoCalc's SD card slot, driven in the card's SPI mode on SPI0.
//
// Every call blocks until the card has finished: a block read takes a fraction
// of a millisecond, and a write can keep the card busy for far longer.
#ifndef YAX86_PICOCALC_SDCARD_H
#define YAX86_PICOCALC_SDCARD_H

#include <stdbool.h>
#include <stdint.h>

enum {
  kSdcardBlockSize = 512,
};

// Whether the slot's card detect switch says a card is in.
bool SdcardIsInserted(void);

// Bring the card up, and return whether it answered. Safe to call again, to
// start over.
bool SdcardInit(void);

// Read or write whole 512 byte blocks, numbered from the start of the card.
bool SdcardReadBlocks(uint32_t block, uint8_t* data, uint32_t count);
bool SdcardWriteBlocks(uint32_t block, const uint8_t* data, uint32_t count);

// Wait for the card to finish the last write.
bool SdcardSync(void);

// The serial clock the card runs at once it is up, in Hz.
uint32_t SdcardGetClockHz(void);

#endif  // YAX86_PICOCALC_SDCARD_H
