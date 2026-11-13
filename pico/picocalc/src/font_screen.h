// Font, from Settings: the 4x10 fonts, of which the one chosen is used at
// once and saved to the card.
#ifndef YAX86_PICOCALC_FONT_SCREEN_H
#define YAX86_PICOCALC_FONT_SCREEN_H

#include "screen.h"
#include "settings.h"

// Show the fonts, with the one settings has chosen. settings must outlive the
// screen: choosing a font sets it there and saves it.
ScreenResult FontScreenOpen(Settings* settings);

#endif  // YAX86_PICOCALC_FONT_SCREEN_H
