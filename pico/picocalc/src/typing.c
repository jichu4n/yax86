#include "typing.h"

enum {
  // Events the queue holds: 128 taps, each a press and a release. A host
  // that sends keys faster than they go is told the queue is full, and sends
  // the key again later.
  kTypingQueueSize = 256,

  kFirstPrintable = 0x20,
  kLastPrintable = 0x7E,
};

typedef struct NamedKey {
  const char* name;
  uint8_t code;
} NamedKey;

static const NamedKey kNamedKeys[] = {
    {"f1", kSouthbridgeKeyCodeF1},
    {"f2", kSouthbridgeKeyCodeF1 + 1},
    {"f3", kSouthbridgeKeyCodeF1 + 2},
    {"f4", kSouthbridgeKeyCodeF1 + 3},
    {"f5", kSouthbridgeKeyCodeF1 + 4},
    {"f6", kSouthbridgeKeyCodeF6},
    {"f7", kSouthbridgeKeyCodeF6 + 1},
    {"f8", kSouthbridgeKeyCodeF6 + 2},
    {"f9", kSouthbridgeKeyCodeF6 + 3},
    {"f10", kSouthbridgeKeyCodeF10},
    {"esc", kSouthbridgeKeyCodeEsc},
    {"enter", kSouthbridgeKeyCodeEnter},
    {"tab", kSouthbridgeKeyCodeTab},
    {"backspace", kSouthbridgeKeyCodeBackspace},
    {"delete", kSouthbridgeKeyCodeDelete},
    {"insert", kSouthbridgeKeyCodeInsert},
    {"home", kSouthbridgeKeyCodeHome},
    {"end", kSouthbridgeKeyCodeEnd},
    {"pageup", kSouthbridgeKeyCodePageUp},
    {"pagedown", kSouthbridgeKeyCodePageDown},
    {"up", kSouthbridgeKeyCodeUp},
    {"down", kSouthbridgeKeyCodeDown},
    {"left", kSouthbridgeKeyCodeLeft},
    {"right", kSouthbridgeKeyCodeRight},
    {"break", kSouthbridgeKeyCodeBreak},
    {"shift", kSouthbridgeKeyCodeLeftShift},
    {"rshift", kSouthbridgeKeyCodeRightShift},
    {"ctrl", kSouthbridgeKeyCodeCtrl},
    {"alt", kSouthbridgeKeyCodeAlt},
    {"capslock", kSouthbridgeKeyCodeCapsLock},
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
