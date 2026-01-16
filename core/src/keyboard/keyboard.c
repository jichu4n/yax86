#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Value for clock_low_ms indicating that a reset has already been triggered.
  kKeyboardResetTriggered = 0xFF,
  // Scan code indicating successful self-test.
  kKeyboardSelfTestOK = 0xAA,
};

void KeyboardInit(KeyboardState* keyboard, KeyboardConfig* config) {
  static const KeyboardState zero_keyboard_state = {0};
  *keyboard = zero_keyboard_state;
  keyboard->config = config;

  // Default to keyboard enabled (enable_clear = false) with clock held low
  // (clock_low = true). This allows us to detect a falling edge on clock_low
  // which triggers the reset timer.
  keyboard->enable_clear = false;
  keyboard->clock_low = true;
  keyboard->clock_low_ms = 0;
  KeyboardBufferInit(&keyboard->buffer);
  keyboard->waiting_for_ack = false;
}

// Helper to send a scancode to the PPI and raise IRQ1 if needed.
static inline void KeyboardSendScancode(
    KeyboardState* keyboard, uint8_t scancode) {
  if (keyboard->config && keyboard->config->send_scancode) {
    keyboard->config->send_scancode(keyboard->config->context, scancode);
  }
  if (keyboard->config && keyboard->config->raise_irq1) {
    keyboard->config->raise_irq1(keyboard->config->context);
  }
  keyboard->waiting_for_ack = true;
}

// Helper to send the next scancode in the buffer if available.
static inline void KeyboardSendNextScancode(KeyboardState* keyboard) {
  // Can only send in state [0, 1], i.e. enable_clear = false clock_low = true
  if (!(keyboard->enable_clear == false && keyboard->clock_low == true)) {
    return;
  }
  // Can only send after previous scancode has been acked.
  if (keyboard->waiting_for_ack) {
    return;
  }
  // If buffer is empty, nothing to send.
  if (KeyboardBufferLength(&keyboard->buffer) == 0) {
    return;
  }

  // Send the next scancode in the buffer.
  uint8_t scancode = *KeyboardBufferGet(&keyboard->buffer, 0);
  KeyboardBufferRemove(&keyboard->buffer, 0);
  KeyboardSendScancode(keyboard, scancode);
}

void KeyboardHandleControl(
    KeyboardState* keyboard, bool enable_clear, bool clock_low) {
  // Save previous state.
  bool old_clock_low = keyboard->clock_low;
  bool old_enable_clear = keyboard->enable_clear;

  // Update state.
  keyboard->enable_clear = enable_clear;
  keyboard->clock_low = clock_low;

  // Falling edge of enable_clear bit indicates ack from BIOS. we clear the
  // waiting_for_ack bit, allowing the next queued scancode to be sent on the
  // next tick.
  if (old_enable_clear == true && keyboard->enable_clear == false &&
      keyboard->clock_low == true) {
    keyboard->waiting_for_ack = false;
  }

  // Falling edge of clock_low bit possibly indicates the start of a reset
  // command from BIOS. We reset the timer at 0ms.
  if (old_clock_low == true && keyboard->clock_low == false) {
    keyboard->clock_low_ms = 0;
  }
}

void KeyboardHandleKeyPress(KeyboardState* keyboard, uint8_t scancode) {
  KeyboardBufferAppend(&keyboard->buffer, &scancode);
}

void KeyboardTickMs(KeyboardState* keyboard) {
  // If clock_low line is being held low, update timer and trigger reset if
  // reached threshold.
  if (keyboard->clock_low == false) {
    if (keyboard->clock_low_ms == kKeyboardResetTriggered) {
      // Reset already triggered, nothing to do.
      return;
    }

    // Increment timer since clock line was held low.
    ++keyboard->clock_low_ms;

    // Haven't reached threshold yet, nothing to do.
    if (keyboard->clock_low_ms < kKeyboardResetThresholdMs) {
      return;
    }

    // Reached threshold, trigger reset.
    KeyboardBufferClear(&keyboard->buffer);
    keyboard->waiting_for_ack = false;
    // Set to special value indicating reset has been triggered.
    keyboard->clock_low_ms = kKeyboardResetTriggered;
    // Send self-test passed scancode on next falling edge of enable_clear.
    uint8_t scancode = kKeyboardSelfTestOK;
    KeyboardBufferAppend(&keyboard->buffer, &scancode);
    return;
  }

  // Normal operation.
  KeyboardSendNextScancode(keyboard);
}

