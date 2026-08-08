#ifndef YAX86_SDL_DISPLAY_H
#define YAX86_SDL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Initialize the display subsystem with a frame buffer of the given size in
// pixels.
bool DisplayInit(int width, int height);

// Clean up the display subsystem.
void DisplayQuit(void);

// Render the current frame to the screen.
void DisplayRender(void);

// Write a pixel to the display buffer.
// This is intended to be called by the core's video callback.
void DisplayPutPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

#endif  // YAX86_SDL_DISPLAY_H
