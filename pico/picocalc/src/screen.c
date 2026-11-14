#include "screen.h"

#include <stddef.h>

#include "pico/stdlib.h"

enum {
  // The deepest the launcher's pages go is six: the list, Manage VMs, a VM,
  // Configure, a drive's configuration and the browser. The in-VM menu's go
  // three deep: the main page, a drive's page and the browser.
  kScreenStackMax = 8,
};

// Markers for the results that open nothing, never shown.
const Screen kScreenBackMarker;
const Screen kScreenCloseMarker;

// The screens showing, the one first shown first and the one on top last.
// Empty while no menu shows.
static const Screen* g_stack[kScreenStackMax];
static uint8_t g_depth;

static const Screen* Top(void) { return g_stack[g_depth - 1]; }

static bool IsScreen(const Screen* screen) {
  return screen != NULL && screen != &kScreenBackMarker &&
         screen != &kScreenCloseMarker;
}

void ScreenShow(ScreenResult opened) {
  if (!IsScreen(opened.screen)) {
    panic("no screen to show");
  }
  g_stack[0] = opened.screen;
  g_depth = 1;
}

void ScreenHide(void) { g_depth = 0; }

bool ScreenIsOpen(void) { return g_depth > 0; }

// Open or take off screens, as result says, and as the screens under one
// taken off say on resuming.
static void Follow(ScreenResult result) {
  while (result.screen == &kScreenBackMarker) {
    if (--g_depth == 0) {
      return;
    }
    result = Top()->on_resume();
  }
  if (result.screen == NULL) {
    return;
  }
  if (result.screen == &kScreenCloseMarker) {
    g_depth = 0;
    return;
  }
  if (g_depth == kScreenStackMax) {
    panic("screens nested past %d", kScreenStackMax);
  }
  g_stack[g_depth++] = result.screen;
}

void ScreenHandleKey(SouthbridgeKeyEvent key) { Follow(Top()->on_key(key)); }

void ScreenTick(void) {
  if (Top()->on_tick != NULL) {
    Follow(Top()->on_tick());
  }
}

bool ScreenDraw(DisplayBox* box, uint64_t now_us) {
  return Top()->on_draw(box, now_us);
}

const char* ScreenHint(void) { return Top()->hint(); }

const char* ScreenPageName(void) { return Top()->page_name(); }
