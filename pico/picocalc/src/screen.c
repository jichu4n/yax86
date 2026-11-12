#include "screen.h"

#include <stddef.h>

#include "ui_status.h"

// The screen showing, or NULL before the list shows and once a VM starts.
static const Screen* g_screen;

void ScreenShow(const Screen* screen) { g_screen = screen; }

void ScreenClose(void) { g_screen = NULL; }

bool ScreenIsOpen(void) { return g_screen != NULL; }

ScreenResult ScreenHandleKey(SouthbridgeKeyEvent key) {
  return g_screen->handle_key(key);
}

ScreenResult ScreenTick(void) { return g_screen->tick(); }

void ScreenDraw(uint64_t now_us) { g_screen->draw(now_us); }

void ScreenUpdateRows(void) { UIStatusOffer(g_screen->hint()); }

const char* ScreenPageName(void) { return g_screen->page_name(); }
