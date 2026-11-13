#include "status_bar.h"

#include <stddef.h>
#include <stdio.h>

enum {
  kAttrWarning = 0x0E,
  kAttrBatteryHigh = 0x0A,
  kAttrBatteryLow = 0x0C,

  // The battery, whose widest - five cells, a space, "100%" and "+" - ends
  // at the row's end.
  kBatteryCells = 5,
  kBatteryColumn = kTextColumns - kBatteryCells - 6,
  kBatteryHighPercent = 50,
  kBatteryLowPercent = 20,

  kPercent = 100,

  // Code page 437's left half block, a battery cell, which in the 4x10 font
  // leaves two columns between one cell and the next, where full blocks or
  // small squares run together.
  kCharBatteryCell = 0xDD,
};

// The problem is kept in two buffers, used in turn, so that a new one is a
// new pointer: the status rows tell a change by its pointer.
static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];

// Status rows composed that core 1 has not been handed yet, because it was
// still drawing the last ones.
static DisplayStatus g_status_rows;
static bool g_status_rows_pending;

void StatusBarSetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

bool StatusBarClearProblem(void) {
  if (g_problem[0] == '\0') {
    return false;
  }
  g_problem[0] = '\0';
  return true;
}

const char* StatusBarProblem(void) {
  return g_problem[0] != '\0' ? g_problem : NULL;
}

void StatusBarClearRow(DisplayTextRow* row) {
  for (uint8_t i = 0; i < kTextColumns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kStatusBarAttrNormal;
  }
}

uint8_t StatusBarPut(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t attr) {
  while (*text != '\0' && column < kTextColumns) {
    row->chars[column] = *text++;
    row->attrs[column] = attr;
    ++column;
  }
  return column;
}

uint8_t StatusBarPutKeys(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t key_attr,
    uint8_t action_attr) {
  uint8_t attr = key_attr;
  for (const char* c = text; *c != '\0' && column < kTextColumns; ++c) {
    if (c[0] == ' ' && c[1] == ' ') {
      attr = key_attr;
    }
    row->chars[column] = *c;
    row->attrs[column] = attr;
    ++column;
    if (*c == '=') {
      attr = action_attr;
    }
  }
  return column;
}

void StatusBarPutBattery(
    const SouthbridgeBattery* battery, DisplayTextRow* row) {
  if (!battery->known) {
    return;
  }
  const uint8_t attr = battery->percent > kBatteryHighPercent ? kAttrBatteryHigh
                       : battery->percent >= kBatteryLowPercent
                           ? kAttrWarning
                           : kAttrBatteryLow;
  // A cell for each fifth of a charge begun, so at least one for any charge.
  const uint8_t filled =
      (uint8_t)((battery->percent * kBatteryCells + kPercent - 1) / kPercent);
  uint8_t column = kBatteryColumn;
  const char cell[] = {(char)kCharBatteryCell, '\0'};
  for (uint8_t i = 0; i < kBatteryCells; ++i) {
    column =
        StatusBarPut(row, column, cell, i < filled ? attr : kStatusBarAttrDim);
  }
  char text[8];
  snprintf(
      text, sizeof(text), " %u%%%s", (unsigned)battery->percent,
      battery->charging ? "+" : "");
  StatusBarPut(row, column, text, attr);
}

DisplayStatus* StatusBarRows(void) { return &g_status_rows; }

void StatusBarHandOver(bool composed) {
  if (composed) {
    g_status_rows_pending = true;
  }
  if (!g_status_rows_pending) {
    return;
  }
  DisplayStatus* rows = DisplayBeginStatus();
  if (rows == NULL) {
    return;
  }
  *rows = g_status_rows;
  DisplayEndStatus();
  g_status_rows_pending = false;
}
