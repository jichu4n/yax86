// The key codes are ClockworkPi's keyboard firmware's
// (Code/picocalc_keyboard in the PicoCalc repository): keyboard.h for the
// codes, and the kbd_entries and btn_entries tables in keyboard.ino for which
// key types which, unshifted and shifted.

#include "keymap.h"

#include <stdbool.h>

enum {
  // A break code is the make code with the top bit set.
  kBreakBit = 0x80,

  // The modifiers, as a PC/XT keyboard's scancodes.
  kScancodeLeftShift = 0x2A,
  kScancodeRightShift = 0x36,
  kScancodeAlt = 0x38,

  kNumKeyCodes = 256,
};

// How the PC types one of the controller's key codes.
typedef struct KeymapEntry {
  // The PC/XT key's make code, or 0 for a key the PC has no key for.
  uint8_t scancode;
  // Whether the controller only reports this key with Shift or Alt held, when
  // on a PC it is a key of its own that either would change.
  bool is_modified_form;
} KeymapEntry;

#define KEY(code) {code, false}
#define MODIFIED_FORM(code) {code, true}

// Laid out a key to a line, both characters it types side by side.
// clang-format off
static const KeymapEntry kKeymap[kNumKeyCodes] = {
    // The number row, unshifted and shifted.
    ['`'] = KEY(0x29), ['~'] = KEY(0x29),
    ['1'] = KEY(0x02), ['!'] = KEY(0x02),
    ['2'] = KEY(0x03), ['@'] = KEY(0x03),
    ['3'] = KEY(0x04), ['#'] = KEY(0x04),
    ['4'] = KEY(0x05), ['$'] = KEY(0x05),
    ['5'] = KEY(0x06), ['%'] = KEY(0x06),
    ['6'] = KEY(0x07), ['^'] = KEY(0x07),
    ['7'] = KEY(0x08), ['&'] = KEY(0x08),
    ['8'] = KEY(0x09), ['*'] = KEY(0x09),
    ['9'] = KEY(0x0A), ['('] = KEY(0x0A),
    ['0'] = KEY(0x0B), [')'] = KEY(0x0B),
    ['-'] = KEY(0x0C), ['_'] = KEY(0x0C),
    ['='] = KEY(0x0D), ['+'] = KEY(0x0D),

    // Letters arrive in lower case, or in upper case with Shift or Caps Lock.
    ['q'] = KEY(0x10), ['Q'] = KEY(0x10),
    ['w'] = KEY(0x11), ['W'] = KEY(0x11),
    ['e'] = KEY(0x12), ['E'] = KEY(0x12),
    ['r'] = KEY(0x13), ['R'] = KEY(0x13),
    ['t'] = KEY(0x14), ['T'] = KEY(0x14),
    ['y'] = KEY(0x15), ['Y'] = KEY(0x15),
    ['u'] = KEY(0x16), ['U'] = KEY(0x16),
    ['i'] = KEY(0x17), ['I'] = KEY(0x17),
    ['o'] = KEY(0x18), ['O'] = KEY(0x18),
    ['p'] = KEY(0x19), ['P'] = KEY(0x19),
    ['['] = KEY(0x1A), ['{'] = KEY(0x1A),
    [']'] = KEY(0x1B), ['}'] = KEY(0x1B),
    ['a'] = KEY(0x1E), ['A'] = KEY(0x1E),
    ['s'] = KEY(0x1F), ['S'] = KEY(0x1F),
    ['d'] = KEY(0x20), ['D'] = KEY(0x20),
    ['f'] = KEY(0x21), ['F'] = KEY(0x21),
    ['g'] = KEY(0x22), ['G'] = KEY(0x22),
    ['h'] = KEY(0x23), ['H'] = KEY(0x23),
    ['j'] = KEY(0x24), ['J'] = KEY(0x24),
    ['k'] = KEY(0x25), ['K'] = KEY(0x25),
    ['l'] = KEY(0x26), ['L'] = KEY(0x26),
    [';'] = KEY(0x27), [':'] = KEY(0x27),
    ['\''] = KEY(0x28), ['"'] = KEY(0x28),
    ['\\'] = KEY(0x2B), ['|'] = KEY(0x2B),
    ['z'] = KEY(0x2C), ['Z'] = KEY(0x2C),
    ['x'] = KEY(0x2D), ['X'] = KEY(0x2D),
    ['c'] = KEY(0x2E), ['C'] = KEY(0x2E),
    ['v'] = KEY(0x2F), ['V'] = KEY(0x2F),
    ['b'] = KEY(0x30), ['B'] = KEY(0x30),
    ['n'] = KEY(0x31), ['N'] = KEY(0x31),
    ['m'] = KEY(0x32), ['M'] = KEY(0x32),
    [','] = KEY(0x33), ['<'] = KEY(0x33),
    ['.'] = KEY(0x34), ['>'] = KEY(0x34),
    ['/'] = KEY(0x35), ['?'] = KEY(0x35),
    [' '] = KEY(0x39),

    // Backspace, Tab and Enter, which the controller codes as ASCII does.
    [0x08] = KEY(0x0E),
    [0x09] = KEY(0x0F),
    [0x0A] = KEY(0x1C),

    // Modifiers.
    [0xA1] = KEY(kScancodeAlt),
    [0xA2] = KEY(kScancodeLeftShift),
    [0xA3] = KEY(kScancodeRightShift),
    [0xA5] = KEY(0x1D),
    [0xC1] = KEY(0x3A),

    // F1 to F5, and F6 to F10, which are their shifted forms.
    [0x81] = KEY(0x3B),
    [0x82] = KEY(0x3C),
    [0x83] = KEY(0x3D),
    [0x84] = KEY(0x3E),
    [0x85] = KEY(0x3F),
    [0x86] = MODIFIED_FORM(0x40),
    [0x87] = MODIFIED_FORM(0x41),
    [0x88] = MODIFIED_FORM(0x42),
    [0x89] = MODIFIED_FORM(0x43),
    [0x90] = MODIFIED_FORM(0x44),

    // Esc, and Break, its shifted form, which is the PC's Scroll Lock key:
    // with Ctrl, Ctrl-Break.
    [0xB1] = KEY(0x01),
    [0xD0] = MODIFIED_FORM(0x46),

    // The arrows, which on a PC/XT are on the keypad, and the keypad keys the
    // PicoCalc has as shifted forms: Page Up and Down on Up and Down, Home on
    // Tab, End on Delete, and Insert on Enter and on Alt and I.
    [0xB5] = KEY(0x48),
    [0xB4] = KEY(0x4B),
    [0xB7] = KEY(0x4D),
    [0xB6] = KEY(0x50),
    [0xD4] = KEY(0x53),
    [0xD2] = MODIFIED_FORM(0x47),
    [0xD6] = MODIFIED_FORM(0x49),
    [0xD5] = MODIFIED_FORM(0x4F),
    [0xD7] = MODIFIED_FORM(0x51),
    [0xD1] = MODIFIED_FORM(0x52),
};
// clang-format on

#undef KEY
#undef MODIFIED_FORM

// The modifiers a modified form is sent without, and whether the guest has
// been told each is held.
static const uint8_t kHiddenModifiers[] = {
    kScancodeLeftShift,
    kScancodeRightShift,
    kScancodeAlt,
};
enum { kNumHiddenModifiers = sizeof(kHiddenModifiers) / sizeof(uint8_t) };
static bool g_is_modifier_held[kNumHiddenModifiers];

// Record what a scancode sent tells the guest about the modifiers.
static void TrackModifiers(uint8_t scancode) {
  for (uint8_t i = 0; i < kNumHiddenModifiers; ++i) {
    if ((scancode & ~kBreakBit) == kHiddenModifiers[i]) {
      g_is_modifier_held[i] = (scancode & kBreakBit) == 0;
    }
  }
}

uint8_t KeymapTranslate(
    SouthbridgeKeyEvent event, uint8_t scancodes[kKeymapMaxNumScancodes]) {
  const KeymapEntry entry = kKeymap[event.key];
  if (entry.scancode == 0) {
    return 0;
  }
  uint8_t num_scancodes = 0;

  if (!entry.is_modified_form) {
    // A key held down repeats as a run of make codes, as a PC/XT keyboard's
    // does.
    scancodes[num_scancodes++] = event.state == kSouthbridgeKeyReleased
                                     ? (uint8_t)(entry.scancode | kBreakBit)
                                     : entry.scancode;
    TrackModifiers(scancodes[0]);
    return num_scancodes;
  }

  // A modified form is a whole key press, sent when it is pressed and again
  // each time it repeats, so that its release has nothing to do. That also
  // covers a release reported as the unmodified key, when the modifier was let
  // go first.
  if (event.state == kSouthbridgeKeyReleased) {
    return 0;
  }
  for (uint8_t i = 0; i < kNumHiddenModifiers; ++i) {
    if (g_is_modifier_held[i]) {
      scancodes[num_scancodes++] = (uint8_t)(kHiddenModifiers[i] | kBreakBit);
    }
  }
  scancodes[num_scancodes++] = entry.scancode;
  scancodes[num_scancodes++] = (uint8_t)(entry.scancode | kBreakBit);
  for (uint8_t i = 0; i < kNumHiddenModifiers; ++i) {
    if (g_is_modifier_held[i]) {
      scancodes[num_scancodes++] = kHiddenModifiers[i];
    }
  }
  return num_scancodes;
}
