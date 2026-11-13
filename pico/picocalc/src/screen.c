#include "screen.h"

#include <stddef.h>

// The screen showing, or NULL while no menu shows.
static const Screen* g_screen;

void ScreenShow(const Screen* screen) { g_screen = screen; }

bool ScreenIsOpen(void) { return g_screen != NULL; }

// Close the screen if result says.
static void Follow(ScreenResult result) {
  if (result == kScreenClose) {
    g_screen = NULL;
  }
}

void ScreenHandleKey(SouthbridgeKeyEvent key) {
  Follow(g_screen->handle_key(key));
}

void ScreenTick(void) { Follow(g_screen->tick()); }

void ScreenDraw(uint64_t now_us) { g_screen->draw(now_us); }

const char* ScreenHint(void) { return g_screen->hint(); }

const char* ScreenPageName(void) { return g_screen->page_name(); }
