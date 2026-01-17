#ifndef YAX86_SDL_DISPLAY_H
#define YAX86_SDL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Initialize the display subsystem.
bool Display_Init(void);

// Clean up the display subsystem.
void Display_Quit(void);

// Render the current frame to the screen.
void Display_Render(void);

// Write a pixel to the display buffer.
// This is intended to be called by the core's video callback.
void Display_PutPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

#endif  // YAX86_SDL_DISPLAY_H
