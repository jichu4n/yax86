#include "typing.h"

enum {
  // Events the queue holds: a little over sixty shifted characters, each a
  // Shift press, a press and a release, and a Shift release.
  kTypingQueueSize = 256,

  // The controller's codes for the keys this types that are not printable
  // ASCII.
  kCodeBackspace = 0x08,
  kCodeTab = 0x09,
  kCodeEnter = 0x0A,
  kCodeAlt = 0xA1,
  kCodeLeftShift = 0xA2,
  kCodeRightShift = 0xA3,
  kCodeCtrl = 0xA5,
  kCodeEsc = 0xB1,
  kCodeLeft = 0xB4,
  kCodeUp = 0xB5,
  kCodeDown = 0xB6,
  kCodeRight = 0xB7,
  kCodeCapsLock = 0xC1,
  kCodeBreak = 0xD0,
  kCodeInsert = 0xD1,
  kCodeHome = 0xD2,
  kCodeDelete = 0xD4,
  kCodeEnd = 0xD5,
  kCodePageUp = 0xD6,
  kCodePageDown = 0xD7,
  kCodeF1 = 0x81,
  kCodeF6 = 0x86,
  kCodeF10 = 0x90,

  kFirstPrintable = 0x20,
  kLastPrintable = 0x7E,
  kEsc = 0x1B,
};

typedef struct NamedKey {
  const char* name;
  uint8_t code;
} NamedKey;

static const NamedKey kNamedKeys[] = {
    {"f1", kCodeF1},
    {"f2", kCodeF1 + 1},
    {"f3", kCodeF1 + 2},
    {"f4", kCodeF1 + 3},
    {"f5", kCodeF1 + 4},
    {"f6", kCodeF6},
    {"f7", kCodeF6 + 1},
    {"f8", kCodeF6 + 2},
    {"f9", kCodeF6 + 3},
    {"f10", kCodeF10},
    {"esc", kCodeEsc},
    {"enter", kCodeEnter},
    {"tab", kCodeTab},
    {"backspace", kCodeBackspace},
    {"delete", kCodeDelete},
    {"insert", kCodeInsert},
    {"home", kCodeHome},
    {"end", kCodeEnd},
    {"pageup", kCodePageUp},
    {"pagedown", kCodePageDown},
    {"up", kCodeUp},
    {"down", kCodeDown},
    {"left", kCodeLeft},
    {"right", kCodeRight},
    {"break", kCodeBreak},
    {"shift", kCodeLeftShift},
    {"rshift", kCodeRightShift},
    {"ctrl", kCodeCtrl},
    {"alt", kCodeAlt},
    {"capslock", kCodeCapsLock},
    {"space", ' '},
};

// The characters typed with Shift held, on the PicoCalc's keyboard as on a
// PC's, other than the capital letters.
static const char kShiftedSymbols[] = "~!@#$%^&*()_+{}|:\"<>?";

static SouthbridgeKeyEvent g_queue[kTypingQueueSize];
static uint16_t g_head;
static uint16_t g_count;

static bool NamesEqual(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

static bool IsShifted(char c) {
  if (c >= 'A' && c <= 'Z') {
    return true;
  }
  for (const char* symbol = kShiftedSymbols; *symbol != '\0'; ++symbol) {
    if (*symbol == c) {
      return true;
    }
  }
  return false;
}

// The controller's code for a character of typed text, if it has one.
static bool CodeForCharacter(char c, uint8_t* code) {
  if (c == '\r' || c == '\n') {
    *code = kCodeEnter;
  } else if (c == '\t') {
    *code = kCodeTab;
  } else if (c == '\b') {
    *code = kCodeBackspace;
  } else if (c == kEsc) {
    *code = kCodeEsc;
  } else if ((uint8_t)c >= kFirstPrintable && (uint8_t)c <= kLastPrintable) {
    *code = (uint8_t)c;
  } else {
    return false;
  }
  return true;
}

static void Push(uint8_t code, SouthbridgeKeyState state) {
  const uint16_t tail = (uint16_t)((g_head + g_count) % kTypingQueueSize);
  g_queue[tail].state = state;
  g_queue[tail].key = code;
  ++g_count;
}

// How many events a key takes: a press and a release, and a Shift press and
// release around them if it needs Shift.
static uint16_t EventsFor(char c) { return IsShifted(c) ? 4 : 2; }

bool TypingQueueText(const char* text) {
  uint16_t needed = 0;
  for (const char* c = text; *c != '\0'; ++c) {
    uint8_t code = 0;
    if (!CodeForCharacter(*c, &code)) {
      return false;
    }
    needed += EventsFor(*c);
  }
  if (needed > kTypingQueueSize - g_count) {
    return false;
  }
  for (const char* c = text; *c != '\0'; ++c) {
    uint8_t code = 0;
    CodeForCharacter(*c, &code);
    const bool shifted = IsShifted(*c);
    if (shifted) {
      Push(kCodeLeftShift, kSouthbridgeKeyPressed);
    }
    Push(code, kSouthbridgeKeyPressed);
    Push(code, kSouthbridgeKeyReleased);
    if (shifted) {
      Push(kCodeLeftShift, kSouthbridgeKeyReleased);
    }
  }
  return true;
}

bool TypingQueueKey(uint8_t code, TypingAction action) {
  const uint16_t needed = action == kTypingTap ? 2 : 1;
  if (needed > kTypingQueueSize - g_count) {
    return false;
  }
  if (action != kTypingRelease) {
    Push(code, kSouthbridgeKeyPressed);
  }
  if (action != kTypingPress) {
    Push(code, kSouthbridgeKeyReleased);
  }
  return true;
}

bool TypingFindKey(const char* name, uint8_t* code) {
  for (uint32_t i = 0; i < sizeof(kNamedKeys) / sizeof(kNamedKeys[0]); ++i) {
    if (NamesEqual(name, kNamedKeys[i].name)) {
      *code = kNamedKeys[i].code;
      return true;
    }
  }
  if (name[0] != '\0' && name[1] == '\0' &&
      (uint8_t)name[0] >= kFirstPrintable &&
      (uint8_t)name[0] <= kLastPrintable) {
    *code = (uint8_t)name[0];
    return true;
  }
  return false;
}

bool TypingNext(SouthbridgeKeyEvent* event) {
  if (g_count == 0) {
    return false;
  }
  *event = g_queue[g_head];
  g_head = (uint16_t)((g_head + 1) % kTypingQueueSize);
  --g_count;
  return true;
}

uint16_t TypingPending(void) { return g_count; }
