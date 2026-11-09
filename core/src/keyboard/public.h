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
  bool waiting_for_ack;

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
