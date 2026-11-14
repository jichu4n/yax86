// ==============================================================================
// YAX86 KEYBOARD MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_KEYBOARD_BUNDLE_H
#define YAX86_KEYBOARD_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/keyboard/public.h start
// ==============================================================================

#line 1 "./src/keyboard/public.h"
// Public interface for the Keyboard module.
#ifndef YAX86_KEYBOARD_PUBLIC_H
#define YAX86_KEYBOARD_PUBLIC_H

// This module emulates a PC/XT keyboard and its interface to the 8255 PPI.
//
// During initialization:
// 1. [0, 0]
//    The BIOS sets both control bits to false and holds them there for at
//    least 20ms. The keyboard detects the clock_low line is held low, and
//    performs a self test. Key presses or releases that arrive while the
//    keyboard is held in reset are dropped rather than buffered.
// 2. -> [1, 1] -> [0, 1]
//    The BIOS restores the clock_low line to true, releasing the reset signal.
//    It pulses the enable_clear line high then low to trigger the next scan
//    code, just like in normal operation.
// 3. The pulse triggers the keyboard to send the self-test OK scancode (0xAA)
//    to the PPI.
// 4. -> [1, 1] -> [0, 1]
//    The BIOS acknowledges the self-test OK scancode by pulsing the
//    enable_clear line again, just like in normal operation.
// 5. -> [1, 1]
//    The BIOS sets both control bits to true to inhibit the keyboard for the
//    rest of the POST process.
// 6. -> [0, 1]
//    At the end of POST, the BIOS enables the keyboard by setting it to normal
//    operational state.
//
// In normal operation:
// 1. [0, 1]
//    In steady state, the control bits are set to enable_clear = false,
//    clock_low = true.
// 2. [0, 1]
//    On key press, the keyboard sends the scancode to the PPI and raises IRQ1.
//    At this point, the control bits are unchanged.
// 3. -> [1, 1] -> [0, 1]
//    The BIOS's IRQ handler sends an ack by briefly pulsing the enable_clear
//    line from false to true to false. This pulse tells the keyboard that it
//    can now send the next scancode.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_KEYBOARD_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_KEYBOARD_BUNDLE_H

enum {
  // Log module ID for the Keyboard.
  Y_KEYBOARD_LOG_MODULE_ID = 7,
};

// Log module for the Keyboard.
Y_PUBLIC_HEADER const YLogModule YKeyboardLogModule = {
    .id = Y_KEYBOARD_LOG_MODULE_ID,
    .name = "keyboard",
};

struct YKeyboardState;

// Caller-provided runtime configuration for the Keyboard.
typedef struct YKeyboardConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to send a scancode to the PPI.
  void (*send_scancode)(void* context, uint8_t scancode);
  // Callback to raise an IRQ1 (keyboard interrupt) to the CPU.
  void (*raise_irq1)(void* context);
} YKeyboardConfig;

enum {
  // Maximum number of keys to buffer. Additional key presses will be dropped.
  Y_KEYBOARD_BUFFER_SIZE = 16,
  // Threshold required to trigger keyboard reset when clock line is held low.
  Y_KEYBOARD_RESET_THRESHOLD_MS = 20,
};
Y_STATIC_VECTOR_TYPE(YKeyboardBuffer, uint8_t, Y_KEYBOARD_BUFFER_SIZE)

// State of the Keyboard.
typedef struct YKeyboardState {
  // Pointer to the keyboard configuration.
  YKeyboardConfig config;

  // State of PPI Port B bit 7, or PBKB in GLaBIOS.
  // - false = enable keyboard
  // - true  = clear keyboard (reset)
  bool enable_clear;

  // Current state of PPI Port B bit 6, or PBKC in GLaBIOS.
  // - false = hold keyboard clock low
  // - true  = enabled (normal operation)
  bool clock_low;

  // Number of ms since the clock_low line was set to false (enabled). This is
  // used to detect the reset signal from the BIOS, which is holding the clock
  // low for at least 20ms.
  //   - 0 = clock line is high (normal operation)
  //   - 0xFF = clock line has been low for at least 20ms
  uint8_t clock_low_ms;

  // Whether we are currently waiting for ack from BIOS before sending the next
  // scancode. The keyboard will not send any further scancodes until the BIOS
  // pulses the enable_clear line high then low.
  bool is_waiting_for_ack;

  // Buffer of key presses received.
  YKeyboardBuffer buffer;
} YKeyboardState;

// Initializes the keyboard to its power-on state.
Y_PUBLIC void YKeyboardInit(YKeyboardState* keyboard);

// Receive keyboard control bits from the PPI (bits 6 and 7 of Port B).
Y_PUBLIC void YKeyboardHandleControl(
    YKeyboardState* keyboard, bool enable_clear, bool clock_low);

// Handles a real key press event.
Y_PUBLIC void YKeyboardHandleKeyPress(
    YKeyboardState* keyboard, uint8_t scancode);

// Simulates a 1ms tick. This is needed to respond to reset commands and to
// send buffered scancodes.
Y_PUBLIC void YKeyboardTickMs(YKeyboardState* keyboard);

#endif  // YAX86_KEYBOARD_PUBLIC_H


// ==============================================================================
// src/keyboard/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/keyboard/keyboard.c start
// ==============================================================================

#line 1 "./src/keyboard/keyboard.c"
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


// ==============================================================================
// src/keyboard/keyboard.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_KEYBOARD_BUNDLE_H

