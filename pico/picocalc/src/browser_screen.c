#include "browser_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "floppy.h"
#include "ui_page.h"

extern const Screen kBrowserScreen;

static UIBrowser g_browser;
// The entry chosen, and whether the page is out of date.
static UIPage g_page;
// Where the image chosen goes, the parent's.
static char* g_path;
static uint32_t* g_size;

ScreenResult BrowserScreenOpen(
    UIBrowserImages images, const char* folder, const char* current,
    char path[kFSMaxPath + 1], uint32_t* size) {
  g_browser.images = images;
  g_browser.current = current;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    g_browser.in_drives[drive] = NULL;
  }
  g_path = path;
  g_size = size;
  g_path[0] = '\0';
  g_page.chosen = UIBrowserOpen(&g_browser, folder);
  g_page.dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

ScreenResult BrowserScreenReopen(void) {
  g_path[0] = '\0';
  g_page.dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

// Open the chosen entry: a folder to look in, or an image, which is the
// parent's.
static ScreenResult OpenEntry(void) {
  uint8_t chosen = g_page.chosen;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (UIBrowserChoose(&g_browser, &chosen, path, &size)) {
    case kUIBrowserNothing:
      return ScreenStays();
    case kUIBrowserOpened:
      g_page.chosen = chosen;
      g_page.dirty = true;
      return ScreenStays();
    case kUIBrowserChoseImage:
      break;
  }
  FSCopyPath(path, g_path);
  *g_size = size;
  return ScreenBack();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, UIBrowserCount(&g_browser))) {
    case kUIPageKeyMoved:
      UIBrowserScrollTo(&g_browser, g_page.chosen);
      return ScreenStays();
    case kUIPageKeyChoose:
      return OpenEntry();
    case kUIPageKeyBack:
      // Backspace goes up a folder, until the card's root.
      if (event.key == kKeyCodeBackspace && !UIBrowserAtRoot(&g_browser)) {
        g_page.chosen = UIBrowserUp(&g_browser);
        g_page.dirty = true;
        return ScreenStays();
      }
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  UIBrowserDraw(&g_browser, menu, g_page.chosen);
  UIPageEndDraw(&g_page);
}

// Nothing opens over the browser.
static ScreenResult Resume(void) {
  g_page.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
}

static const char* PageName(void) { return "browse"; }

const Screen kBrowserScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
