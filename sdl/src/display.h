#ifndef YAX86_SDL_DISPLAY_H
#define YAX86_SDL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "core/video.h"

// Initialize the display subsystem with a frame buffer of the given size in
// pixels.
bool DisplayInit(int width, int height);

// Clean up the display subsystem.
void DisplayQuit(void);

// Begin and end a rectangular update to the retained display buffer. Pixels
// between these calls are written in row-major order.
void DisplayBeginRegion(int x, int y, int width, int height);
void DisplayEndRegion(void);

// Render the current frame to the screen.
void DisplayRender(void);

// Write a horizontal RGB pixel span to the retained display buffer.
void DisplayPutPixels(int x, int y, const RGB* pixels, uint8_t count);

#endif  // YAX86_SDL_DISPLAY_H
