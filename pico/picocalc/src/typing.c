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
    {"f1", kKeyCodeF1},
    {"f2", kKeyCodeF1 + 1},
    {"f3", kKeyCodeF1 + 2},
    {"f4", kKeyCodeF1 + 3},
    {"f5", kKeyCodeF1 + 4},
    {"f6", kKeyCodeF6},
    {"f7", kKeyCodeF6 + 1},
    {"f8", kKeyCodeF6 + 2},
    {"f9", kKeyCodeF6 + 3},
    {"f10", kKeyCodeF10},
    {"esc", kKeyCodeEsc},
    {"enter", kKeyCodeEnter},
    {"tab", kKeyCodeTab},
    {"backspace", kKeyCodeBackspace},
    {"delete", kKeyCodeDelete},
    {"insert", kKeyCodeInsert},
    {"home", kKeyCodeHome},
    {"end", kKeyCodeEnd},
    {"pageup", kKeyCodePageUp},
    {"pagedown", kKeyCodePageDown},
    {"up", kKeyCodeUp},
    {"down", kKeyCodeDown},
    {"left", kKeyCodeLeft},
    {"right", kKeyCodeRight},
    {"break", kKeyCodeBreak},
    {"shift", kKeyCodeLeftShift},
    {"rshift", kKeyCodeRightShift},
    {"ctrl", kKeyCodeCtrl},
    {"alt", kKeyCodeAlt},
    {"capslock", kKeyCodeCapsLock},
    {"space", ' '},
};

static SouthbridgeKeyEvent g_queue[kTypingQueueSize];
static uint16_t g_head;
static uint16_t g_num_queued;

static bool AreNamesEqual(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

static void Push(uint8_t code, SouthbridgeKeyState state) {
  const uint16_t tail = (uint16_t)((g_head + g_num_queued) % kTypingQueueSize);
  g_queue[tail].state = state;
  g_queue[tail].key = code;
  ++g_num_queued;
}

bool TypingQueueKey(uint8_t code, TypingAction action) {
  const uint16_t needed = action == kTypingTap ? 2 : 1;
  if (needed > kTypingQueueSize - g_num_queued) {
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
    if (AreNamesEqual(name, kNamedKeys[i].name)) {
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
  if (g_num_queued == 0) {
    return false;
  }
  *event = g_queue[g_head];
  g_head = (uint16_t)((g_head + 1) % kTypingQueueSize);
  --g_num_queued;
  return true;
}
