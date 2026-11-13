#include "browser_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "floppy.h"
#include "status_bar.h"
#include "ui_page.h"

extern const Screen kBrowserScreen;

static UIBrowser g_browser;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
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
  g_chosen = UIBrowserOpen(&g_browser, folder);
  g_dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

ScreenResult BrowserScreenReopen(void) {
  g_path[0] = '\0';
  g_dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

// Open the chosen entry: a folder to look in, or an image, which is the
// parent's.
static ScreenResult OpenEntry(void) {
  uint8_t chosen = g_chosen;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (UIBrowserChoose(&g_browser, &chosen, path, &size)) {
    case kUIBrowserNothing:
      return ScreenStays();
    case kUIBrowserOpened:
      g_chosen = chosen;
      g_dirty = true;
      return ScreenStays();
    case kUIBrowserChoseImage:
      break;
  }
  FSCopyPath(path, g_path);
  *g_size = size;
  return ScreenBack();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the entries as a press
  // does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  StatusBarClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, UIBrowserCount(&g_browser), &g_chosen)) {
      g_dirty = true;
    }
    UIBrowserScrollTo(&g_browser, g_chosen);
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return OpenEntry();
    case kKeyCodeBackspace:
      // Up a folder, until the card's root.
      if (!UIBrowserAtRoot(&g_browser)) {
        g_chosen = UIBrowserUp(&g_browser);
        g_dirty = true;
        return ScreenStays();
      }
      return ScreenBack();
    case kKeyCodeEsc:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  UIBrowserDraw(&g_browser, menu, g_chosen);
  DisplayEndMenu();
  g_dirty = false;
}

// Nothing opens over the browser.
static ScreenResult Resume(void) {
  g_dirty = true;
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
