#include "screen.h"

#include <stddef.h>

#include "pico/stdlib.h"

enum {
  // The deepest the pages go is six: the list, Manage VMs, a VM, Edit
  // drives, a drive's choices and the browser.
  kScreenStackMax = 8,
};

// The screens showing, the list first and the one on top last. Empty before
// the list shows and once a VM starts.
static const Screen* g_stack[kScreenStackMax];
static uint8_t g_depth;

static const Screen* Top(void) { return g_stack[g_depth - 1]; }

void ScreenShow(const Screen* screen) {
  g_stack[0] = screen;
  g_depth = 1;
}

void ScreenClose(void) { g_depth = 0; }

bool ScreenIsOpen(void) { return g_depth > 0; }

// Open or take off a screen, as result says.
static ScreenResult Follow(ScreenResult result) {
  switch (result.action) {
    case kScreenOpen:
      if (g_depth == kScreenStackMax) {
        panic("screens nested past %d", kScreenStackMax);
      }
      g_stack[g_depth++] = result.screen;
      break;
    case kScreenBack:
      // The list is never taken off: nothing closes it but starting a VM.
      if (g_depth > 1) {
        --g_depth;
        Top()->resume();
      }
      break;
    case kScreenStays:
    case kScreenChoseVM:
      break;
  }
  return result;
}

ScreenResult ScreenHandleKey(SouthbridgeKeyEvent key) {
  return Follow(Top()->handle_key(key));
}

ScreenResult ScreenTick(void) {
  if (Top()->tick == NULL) {
    return ScreenStays();
  }
  return Follow(Top()->tick());
}

void ScreenDraw(uint64_t now_us) { Top()->draw(now_us); }

const char* ScreenHint(void) { return Top()->hint(); }

const char* ScreenPageName(void) { return Top()->page_name(); }
