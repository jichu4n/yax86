#include "ui_status.h"

#include <stddef.h>
#include <stdio.h>

#include "southbridge.h"
#include "status_bar.h"
#include "text.h"

// The problem is kept in two buffers, used in turn, so that a new one is a
// new pointer: the status rows tell a change of hint by its pointer.
static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];

void UIStatusSetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

bool UIStatusClearProblem(void) {
  if (g_problem[0] == '\0') {
    return false;
  }
  g_problem[0] = '\0';
  return true;
}

void UIStatusOffer(const char* hint) {
  StatusBarState state = {0};
  state.battery = SouthbridgeGetBattery();
  state.hint = hint;
  state.problem = g_problem[0] != '\0' ? g_problem : NULL;
  StatusBarOffer(&state);
}
