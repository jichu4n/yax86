#include "speaker.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

enum {
  // The audio pins: channels A and B of one PWM slice, left and right.
  kSpeakerPinLeft = 26,
  kSpeakerPinRight = 27,

  // The highest tone played. The PIT can be set to anything up to its
  // 1.19MHz input clock, and nothing above about 20kHz is heard, so a tone
  // higher than this is played as silence rather than handed to the
  // amplifier.
  kSpeakerMaxHz = 20000,
  // The PWM counter's largest top, and the divider's largest integer part.
  // At 250MHz they reach down to 15Hz, below the PIT's lowest tone of 18Hz.
  kSpeakerMaxCount = 65536,
  kSpeakerMaxDivider = 255,
};

static uint g_slice;
static SpeakerStats g_stats;

static void Silence(void) {
  // Both pins held low, so the speakers rest where they do with no signal.
  pwm_set_both_levels(g_slice, 0, 0);
}

void SpeakerInit(void) {
  gpio_set_function(kSpeakerPinLeft, GPIO_FUNC_PWM);
  gpio_set_function(kSpeakerPinRight, GPIO_FUNC_PWM);
  g_slice = pwm_gpio_to_slice_num(kSpeakerPinLeft);
  pwm_config config = pwm_get_default_config();
  pwm_init(g_slice, &config, false);
  Silence();
  pwm_set_enabled(g_slice, true);
}

void SpeakerSetFrequency(uint32_t frequency_hz) {
  ++g_stats.changes;
  g_stats.frequency_hz = frequency_hz;
  if (frequency_hz == 0 || frequency_hz > kSpeakerMaxHz) {
    Silence();
    g_stats.output_hz = 0;
    return;
  }
  // The smallest divider that lets the counter's period reach the tone, for
  // the finest period, and the period that comes nearest.
  const uint32_t sys_hz = clock_get_hz(clk_sys);
  uint32_t divider = sys_hz / (frequency_hz * kSpeakerMaxCount) + 1;
  if (divider > kSpeakerMaxDivider) {
    divider = kSpeakerMaxDivider;
  }
  uint32_t count = (sys_hz / divider + frequency_hz / 2) / frequency_hz;
  if (count > kSpeakerMaxCount) {
    count = kSpeakerMaxCount;
  }
  pwm_set_clkdiv_int_frac(g_slice, (uint8_t)divider, 0);
  pwm_set_wrap(g_slice, (uint16_t)(count - 1));
  // A square wave: high for half the period.
  pwm_set_both_levels(g_slice, (uint16_t)(count / 2), (uint16_t)(count / 2));
  const uint32_t period = divider * count;
  g_stats.output_hz = (sys_hz + period / 2) / period;
}

SpeakerStats SpeakerGetStats(void) { return g_stats; }
