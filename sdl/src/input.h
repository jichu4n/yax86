#ifndef YAX86_SDL_INPUT_H
#define YAX86_SDL_INPUT_H

#include <SDL3/SDL.h>
#include "core/platform.h"

// Handle SDL events (keyboard, etc.) and update platform state.
void InputHandleEvent(const SDL_Event* event, PlatformState* platform);

#endif  // YAX86_SDL_INPUT_H
