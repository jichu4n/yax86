#ifndef YAX86_VIDEO_FONTS_H
#define YAX86_VIDEO_FONTS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// MDA 9x14 font bitmaps.
extern const uint16_t kFontMDA9x14Bitmap[256][14];
// CGA 8x8 font bitmaps.
extern const uint8_t kFontCGA8x8Bitmap[256][8];

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_VIDEO_FONTS_H
