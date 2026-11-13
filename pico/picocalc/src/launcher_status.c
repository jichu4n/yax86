#include "launcher_status.h"

#include <stdbool.h>
#include <stddef.h>

#include "southbridge.h"
#include "status_bar.h"

// What the rows were last composed from, so that they are composed again
// only when it changes.
typedef struct ComposedFrom {
  SouthbridgeBattery battery;
  const char* hint;
  const char* problem;
} ComposedFrom;

static ComposedFrom g_composed_from;
static bool g_composed;

static bool ComposedFromEqual(const ComposedFrom* a, const ComposedFrom* b) {
  return a->battery.known == b->battery.known &&
         a->battery.percent == b->battery.percent &&
         a->battery.charging == b->battery.charging && a->hint == b->hint &&
         a->problem == b->problem;
}

void LauncherStatusOffer(const char* hint) {
  ComposedFrom from;
  from.battery = SouthbridgeGetBattery();
  from.hint = hint;
  from.problem = StatusBarProblem();
  const bool composed =
      !g_composed || !ComposedFromEqual(&from, &g_composed_from);
  if (composed) {
    g_composed_from = from;
    g_composed = true;
    DisplayStatus* rows = StatusBarRows();
    for (uint8_t row = 0; row < kDisplayStatusRows; ++row) {
      StatusBarClearRow(&rows->rows[row]);
    }
    DisplayTextRow* messages = &rows->rows[kStatusBarRowMessages];
    if (from.problem != NULL) {
      StatusBarPut(messages, 0, from.problem, kStatusBarAttrBright);
    } else if (from.hint != NULL) {
      StatusBarPutKeys(
          messages, 0, from.hint, kStatusBarAttrNormal, kStatusBarAttrBright);
    }
    StatusBarPutBattery(&from.battery, &rows->rows[kStatusBarRowMachine]);
  }
  StatusBarHandOver(composed);
}
