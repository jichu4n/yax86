#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Value for clock_low_ms indicating that a reset has already been triggered.
  Y_KEYBOARD_RESET_TRIGGERED = 0xFF,
  // Scan code indicating successful self-test.
  Y_KEYBOARD_SELF_TEST_OK = 0xAA,
};

Y_PUBLIC void YKeyboardInit(YKeyboardState* keyboard) {
  // Default to keyboard enabled (enable_clear = false) with clock held low
  // (clock_low = true). This allows us to detect a falling edge on clock_low
  // which triggers the reset timer.
  keyboard->enable_clear = false;
  keyboard->clock_low = true;
  keyboard->clock_low_ms = 0;
  YKeyboardBufferInit(&keyboard->buffer);
  keyboard->is_waiting_for_ack = false;
}

// Helper to send a scancode to the PPI and raise IRQ1 if needed.
Y_FILE_PRIVATE inline void YKeyboardSendScancode(
    YKeyboardState* keyboard, uint8_t scancode) {
  if (keyboard->config.send_scancode) {
    keyboard->config.send_scancode(keyboard->config.context, scancode);
  }
  if (keyboard->config.raise_irq1) {
    keyboard->config.raise_irq1(keyboard->config.context);
  }
  keyboard->is_waiting_for_ack = true;
}

// Helper to send the next scancode in the buffer if available.
Y_FILE_PRIVATE inline void YKeyboardSendNextScancode(YKeyboardState* keyboard) {
  // Can only send in state [0, 1], i.e. enable_clear = false clock_low = true
  if (!(keyboard->enable_clear == false && keyboard->clock_low == true)) {
    return;
  }
  // Can only send after previous scancode has been acked.
  if (keyboard->is_waiting_for_ack) {
    return;
  }
  // If buffer is empty, nothing to send.
  if (YKeyboardBufferLength(&keyboard->buffer) == 0) {
    return;
  }

  // Send the next scancode in the buffer.
  uint8_t scancode = *YKeyboardBufferGet(&keyboard->buffer, 0);
  YKeyboardBufferRemove(&keyboard->buffer, 0);
  YKeyboardSendScancode(keyboard, scancode);
}

Y_PUBLIC void YKeyboardHandleControl(
    YKeyboardState* keyboard, bool enable_clear, bool clock_low) {
  // Save previous state.
  bool old_clock_low = keyboard->clock_low;
  bool old_enable_clear = keyboard->enable_clear;

  // Update state.
  keyboard->enable_clear = enable_clear;
  keyboard->clock_low = clock_low;

  // Falling edge of enable_clear bit indicates ack from BIOS. we clear the
  // is_waiting_for_ack bit, allowing the next queued scancode to be sent on the
  // next tick.
  if (old_enable_clear == true && keyboard->enable_clear == false &&
      keyboard->clock_low == true) {
    keyboard->is_waiting_for_ack = false;
  }

  // Falling edge of clock_low bit possibly indicates the start of a reset
  // command from BIOS. We reset the timer at 0ms.
  if (old_clock_low == true && keyboard->clock_low == false) {
    keyboard->clock_low_ms = 0;
  }
}

Y_PUBLIC void YKeyboardHandleKeyPress(
    YKeyboardState* keyboard, uint8_t scancode) {
  // Drop key presses that occur while the keyboard is running its self test.
  // Queueing it would let it resurface once the reset ends, which lands it in
  // the middle of the BIOS's stuck key test.
  if (!keyboard->clock_low &&
      keyboard->clock_low_ms == Y_KEYBOARD_RESET_TRIGGERED) {
    return;
  }
  YKeyboardBufferAppend(&keyboard->buffer, &scancode);
}

Y_PUBLIC void YKeyboardTickMs(YKeyboardState* keyboard) {
  // If clock_low line is being held low, update timer and trigger reset if
  // reached threshold.
  if (keyboard->clock_low == false) {
    if (keyboard->clock_low_ms == Y_KEYBOARD_RESET_TRIGGERED) {
      // Reset already triggered, nothing to do.
      return;
    }

    // Increment timer since clock line was held low.
    ++keyboard->clock_low_ms;

    // Haven't reached threshold yet, nothing to do.
    if (keyboard->clock_low_ms < Y_KEYBOARD_RESET_THRESHOLD_MS) {
      return;
    }

    // Reached threshold, trigger reset.
    YKeyboardBufferClear(&keyboard->buffer);
    keyboard->is_waiting_for_ack = false;
    // Set to special value indicating reset has been triggered.
    keyboard->clock_low_ms = Y_KEYBOARD_RESET_TRIGGERED;
    // Send self-test passed scancode on next falling edge of enable_clear.
    uint8_t scancode = Y_KEYBOARD_SELF_TEST_OK;
    YKeyboardBufferAppend(&keyboard->buffer, &scancode);
    return;
  }

  // Normal operation.
  YKeyboardSendNextScancode(keyboard);
}
