#include "launcher_status_bar.h"

#include <stdbool.h>
#include <stddef.h>

#include "southbridge.h"
#include "status_bar.h"

// What the rows show. The rows are composed again only when this changes.
typedef struct RowContents {
  // The battery's charge.
  SouthbridgeBattery battery;
  // The keys of the screen showing, or NULL.
  const char* hint;
  // The problem showing, or NULL. A new problem is a new pointer.
  const char* problem;
} RowContents;

// What the rows showed when they were last composed, and whether they have
// been.
static RowContents g_last_contents;
static bool g_has_last_contents;

static bool RowContentsEqual(const RowContents* a, const RowContents* b) {
  return a->battery.known == b->battery.known &&
         a->battery.percent == b->battery.percent &&
         a->battery.charging == b->battery.charging && a->hint == b->hint &&
         a->problem == b->problem;
}

void LauncherStatusBarUpdate(const char* hint) {
  RowContents contents;
  contents.battery = SouthbridgeGetBattery();
  contents.hint = hint;
  contents.problem = StatusBarProblem();
  const bool changed =
      !g_has_last_contents || !RowContentsEqual(&contents, &g_last_contents);
  if (changed) {
    g_last_contents = contents;
    g_has_last_contents = true;
    DisplayStatus* rows = StatusBarRows();
    for (uint8_t row = 0; row < kDisplayStatusRows; ++row) {
      StatusBarClearRow(&rows->rows[row]);
    }
    DisplayTextRow* messages = &rows->rows[kStatusBarRowMessages];
    if (contents.problem != NULL) {
      StatusBarPut(messages, 0, contents.problem, kStatusBarAttrBright);
    } else if (contents.hint != NULL) {
      StatusBarPutKeys(
          messages, 0, contents.hint, kStatusBarAttrNormal,
          kStatusBarAttrBright);
    }
    StatusBarPutBattery(&contents.battery, &rows->rows[kStatusBarRowDevices]);
  }
  StatusBarSend(changed);
}
