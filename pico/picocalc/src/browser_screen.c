#include "browser_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "floppy.h"
#include "list_box.h"

extern const Screen kBrowserScreen;

static FileBrowser g_browser;
// The list box: the entry selected, and whether it is out of date.
static ListBox g_list_box;
// Where the image selected goes, the parent's.
static char* g_path;
static uint32_t* g_size;

ScreenResult BrowserScreenOpen(
    FileBrowserImages images, const char* folder, const char* current,
    char path[kFSMaxPath + 1], uint32_t* size) {
  g_browser.images = images;
  g_browser.current = current;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    g_browser.in_drives[drive] = NULL;
  }
  g_path = path;
  g_size = size;
  g_path[0] = '\0';
  g_list_box.selected = FileBrowserOpen(&g_browser, folder);
  g_list_box.dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

ScreenResult BrowserScreenReopen(void) {
  g_path[0] = '\0';
  g_list_box.dirty = true;
  return ScreenOpen(&kBrowserScreen);
}

// Open the selected entry: a folder to look in, or an image, which is the
// parent's.
static ScreenResult OpenEntry(void) {
  uint8_t selected = g_list_box.selected;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (FileBrowserSelect(&g_browser, &selected, path, &size)) {
    case kFileBrowserNothing:
      return ScreenStays();
    case kFileBrowserOpenedFolder:
      g_list_box.selected = selected;
      g_list_box.dirty = true;
      return ScreenStays();
    case kFileBrowserSelectedImage:
      break;
  }
  FSCopyPath(path, g_path);
  *g_size = size;
  return ScreenBack();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, FileBrowserCount(&g_browser))) {
    case kListBoxKeyMoved:
      FileBrowserScrollTo(&g_browser, g_list_box.selected);
      return ScreenStays();
    case kListBoxKeySelect:
      return OpenEntry();
    case kListBoxKeyBack:
      // Backspace goes up a folder, until the card's root.
      if (event.key == kKeyCodeBackspace && !FileBrowserAtRoot(&g_browser)) {
        g_list_box.selected = FileBrowserUp(&g_browser);
        g_list_box.dirty = true;
        return ScreenStays();
      }
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  FileBrowserDraw(&g_browser, menu, g_list_box.selected);
  ListBoxEndDraw(&g_list_box);
}

// Nothing opens over the browser.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
}

static const char* PageName(void) { return "browse"; }

const Screen kBrowserScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
