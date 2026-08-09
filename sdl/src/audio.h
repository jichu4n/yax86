#ifndef YAX86_SDL_AUDIO_H
#define YAX86_SDL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

// Initialize the audio subsystem. Returns false if audio is unavailable, in
// which case the emulator still runs, just silently.
bool AudioInit(void);

// Clean up the audio subsystem.
void AudioQuit(void);

// Set the tone the PC speaker emits, in Hz, or 0 to silence it. This is
// intended to be called by the core's PC speaker callback, and is safe to call
// from the emulation loop while the audio callback is running.
void AudioSetFrequency(uint32_t frequency_hz);

// Resume the audio device. Browsers refuse to start audio until the user has
// interacted with the page, so this is called again on the first input event.
// Harmless to call repeatedly.
void AudioResume(void);

#endif  // YAX86_SDL_AUDIO_H
