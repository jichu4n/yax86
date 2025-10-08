// ==============================================================================
// YAX86 KEYBOARD MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_KEYBOARD_BUNDLE_H
#define YAX86_KEYBOARD_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)          \
  typedef struct name {                                                   \
    StaticVectorHeader header;                                            \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const StaticVectorHeader header = {                            \
        .element_size = sizeof(element_type),                             \
        .max_length = (max_length_value),                                 \
        .length = 0,                                                      \
    };                                                                    \
    vector->header = header;                                              \
  }                                                                       \
  static size_t name##Length(const name* vector) __attribute__((unused)); \
  static size_t name##Length(const name* vector) {                        \
    return vector->header.length;                                         \
  }                                                                       \
  static element_type* name##Get(name* vector, size_t index)              \
      __attribute__((unused));                                            \
  static element_type* name##Get(name* vector, size_t index) {            \
    if (index >= (max_length_value)) {                                    \
      return NULL;                                                        \
    }                                                                     \
    return &(vector->elements[index]);                                    \
  }                                                                       \
  static bool name##Append(name* vector, const element_type* element)     \
      __attribute__((unused));                                            \
  static bool name##Append(name* vector, const element_type* element) {   \
    if (vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    vector->elements[vector->header.length++] = *element;                 \
    return true;                                                          \
  }                                                                       \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element)            \
      __attribute__((unused));                                            \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element) {          \
    if (index > vector->header.length ||                                  \
        vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    for (size_t i = vector->header.length; i > index; --i) {              \
      vector->elements[i] = vector->elements[i - 1];                      \
    }                                                                     \
    vector->elements[index] = *element;                                   \
    ++vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static bool name##Remove(name* vector, size_t index)                    \
      __attribute__((unused));                                            \
  static bool name##Remove(name* vector, size_t index) {                  \
    if (index >= vector->header.length) {                                 \
      return false;                                                       \
    }                                                                     \
    for (size_t i = index; i < vector->header.length - 1; ++i) {          \
      vector->elements[i] = vector->elements[i + 1];                      \
    }                                                                     \
    --vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static void name##Clear(name* vector) __attribute__((unused));          \
  static void name##Clear(name* vector) { vector->header.length = 0; }

#endif  // YAX86_UTIL_STATIC_VECTOR_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================

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
//    performs a self test.
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
#include "../util/static_vector.h"
#endif  // YAX86_KEYBOARD_BUNDLE_H

struct KeyboardState;

// Caller-provided runtime configuration for the Keyboard.
typedef struct KeyboardConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Callback to send a scancode to the PPI.
  void (*send_scancode)(void* context, uint8_t scancode);
  // Callback to raise an IRQ1 (keyboard interrupt) to the CPU.
  void (*raise_irq1)(void* context);
} KeyboardConfig;

enum {
  // Maximum number of keys to buffer. Additional key presses will be dropped.
  kKeyboardBufferSize = 16,
  // Threshold required to trigger keyboard reset when clock line is held low.
  kKeyboardResetThresholdMs = 20,
};
STATIC_VECTOR_TYPE(KeyboardBuffer, uint8_t, kKeyboardBufferSize)

// State of the Keyboard.
typedef struct KeyboardState {
  // Pointer to the keyboard configuration.
  KeyboardConfig* config;

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
  KeyboardBuffer buffer;
} KeyboardState;

// Initializes the keyboard to its power-on state.
void KeyboardInit(KeyboardState* keyboard, KeyboardConfig* config);

// Receive keyboard control bits from the PPI (bits 6 and 7 of Port B).
void KeyboardHandleControl(
    KeyboardState* keyboard, bool enable_clear, bool clock_low);

// Handles a real key press event.
void KeyboardHandleKeyPress(KeyboardState* keyboard, uint8_t scancode);

// Simulates a 1ms tick. This is needed to respond to reset commands and to
// send buffered scancodes.
void KeyboardTickMs(KeyboardState* keyboard);

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



// ==============================================================================
// src/keyboard/keyboard.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_KEYBOARD_BUNDLE_H

