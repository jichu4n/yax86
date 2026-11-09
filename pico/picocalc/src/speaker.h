// The PC speaker, on the PicoCalc's two speakers.
//
// The core reports the speaker as the frequency of the square wave it should
// make, or 0 for silence, and a PWM slice makes exactly that: the slice's own
// period set to the tone and its duty to half, on the two audio pins, which
// the mainboard filters and amplifies. Nothing runs per sample or per batch.
#ifndef YAX86_PICOCALC_SPEAKER_H
#define YAX86_PICOCALC_SPEAKER_H

#include <stdint.h>

// What the speaker is doing, and has done, for the report.
typedef struct SpeakerStats {
  // The tone the guest asks for, in Hz, or 0 for none.
  uint32_t frequency_hz;
  // The tone being played: the asked for one as near as the system clock
  // divides to, or 0 when silent, which a tone too high to hear is as well.
  uint32_t output_hz;
  // How many times the tone has changed, silence included.
  uint32_t changes;
} SpeakerStats;

// Bring the audio pins up, silent. Call after the system clock is set.
void SpeakerInit(void);

// Play a square wave at this frequency, or nothing for 0.
void SpeakerSetFrequency(uint32_t frequency_hz);

SpeakerStats SpeakerGetStats(void);

#endif  // YAX86_PICOCALC_SPEAKER_H
