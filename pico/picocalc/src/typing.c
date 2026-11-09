#include "typing.h"

enum {
  // Events the queue holds: 128 taps, each a press and a release. A host
  // that sends keys faster than they go is told the queue is full, and sends
  // the key again later.
  kTypingQueueSize = 256,

  // The controller's codes for the named keys that are not printable ASCII.
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

static void Push(uint8_t code, SouthbridgeKeyState state) {
  const uint16_t tail = (uint16_t)((g_head + g_count) % kTypingQueueSize);
  g_queue[tail].state = state;
  g_queue[tail].key = code;
  ++g_count;
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
