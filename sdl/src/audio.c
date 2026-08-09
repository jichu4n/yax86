#include "audio.h"

#include <SDL3/SDL.h>

enum {
  // Output sample rate in Hz.
  kSampleRate = 48000,
  // Number of samples generated per pass through the mixing loop.
  kBufferSamples = 512,

  // Peak amplitude of the square wave. The PC speaker has one volume, so this
  // is a matter of taste: loud enough to hear over a quiet room, quiet enough
  // that a POST beep is not startling.
  kAmplitude = 6000,
  // How long the amplitude takes to reach its target, in samples. Cutting a
  // square wave off mid-cycle at full amplitude produces a discontinuity that
  // is louder than the click a real speaker makes, so the amplitude is ramped
  // instead. A millisecond is short enough to be imperceptible as a fade.
  kRampSamples = kSampleRate / 1000,
  // Amplitude change per sample while ramping.
  kAmplitudeStep = kAmplitude / kRampSamples,

  // Frequencies at or above this cannot be represented at this sample rate and
  // would alias into noise, so they are treated as silence. The PIT can be
  // programmed well past it - a reload value of 1 asks for 1.19MHz.
  kMaxFrequencyHz = kSampleRate / 2,
};

static SDL_AudioStream* g_stream = NULL;

// The tone the speaker should emit, in Hz, or 0 for silence. Written by the
// emulation loop and read by the audio callback, which runs on its own thread.
static SDL_AtomicInt g_frequency_hz;

// Position within the square wave, as a fraction of a full cycle scaled to the
// range of a uint32_t. Never reset, so a change of frequency does not produce
// a discontinuity.
static uint32_t g_phase = 0;
// Current amplitude, which follows the target set by the frequency.
static int32_t g_amplitude = 0;

static int16_t g_buffer[kBufferSamples];

// Generates one buffer's worth of square wave and returns how many samples it
// produced.
static int AudioGenerate(int num_samples) {
  if (num_samples > kBufferSamples) {
    num_samples = kBufferSamples;
  }

  const int frequency_hz = SDL_GetAtomicInt(&g_frequency_hz);
  // How far through the cycle each sample advances.
  const uint32_t phase_step =
      frequency_hz > 0
          ? (uint32_t)(((uint64_t)frequency_hz << 32) / (uint64_t)kSampleRate)
          : 0;
  const int32_t target_amplitude = frequency_hz > 0 ? kAmplitude : 0;

  for (int i = 0; i < num_samples; ++i) {
    if (g_amplitude < target_amplitude) {
      g_amplitude += kAmplitudeStep;
      if (g_amplitude > target_amplitude) {
        g_amplitude = target_amplitude;
      }
    } else if (g_amplitude > target_amplitude) {
      g_amplitude -= kAmplitudeStep;
      if (g_amplitude < target_amplitude) {
        g_amplitude = target_amplitude;
      }
    }

    // The speaker is driven by a square wave, so the sample is one of two
    // levels depending on which half of the cycle we are in. While silent the
    // phase is frozen, which lets the amplitude ramp settle the level to zero
    // rather than cutting it off.
    g_phase += phase_step;
    g_buffer[i] =
        (int16_t)((g_phase & 0x80000000u) ? g_amplitude : -g_amplitude);
  }

  return num_samples;
}

static void SDLCALL AudioCallback(
    void* userdata, SDL_AudioStream* stream, int additional_amount,
    int total_amount) {
  (void)userdata;
  (void)total_amount;

  int samples_remaining = additional_amount / (int)sizeof(int16_t);
  while (samples_remaining > 0) {
    const int num_samples = AudioGenerate(samples_remaining);
    SDL_PutAudioStreamData(
        stream, g_buffer, num_samples * (int)sizeof(int16_t));
    samples_remaining -= num_samples;
  }
}

bool AudioInit(void) {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_Log("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
    return false;
  }

  SDL_AudioSpec spec = {
      .format = SDL_AUDIO_S16,
      .channels = 1,
      .freq = kSampleRate,
  };
  g_stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, NULL);
  if (!g_stream) {
    SDL_Log("SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return false;
  }

  AudioResume();
  return true;
}

void AudioQuit(void) {
  if (g_stream) {
    // Destroying the stream closes the device, which stops the sound whether
    // or not the emulated machine got around to switching the speaker off.
    SDL_DestroyAudioStream(g_stream);
    g_stream = NULL;
  }
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioSetFrequency(uint32_t frequency_hz) {
  const int value = frequency_hz > 0 && frequency_hz < kMaxFrequencyHz
                        ? (int)frequency_hz
                        : 0;
  SDL_SetAtomicInt(&g_frequency_hz, value);
}

void AudioResume(void) {
  if (g_stream) {
    SDL_ResumeAudioStreamDevice(g_stream);
  }
}
