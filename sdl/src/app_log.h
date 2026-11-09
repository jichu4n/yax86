#ifndef YAX86_SDL_APP_LOG_H
#define YAX86_SDL_APP_LOG_H

// The SDL runtime's own log module, and the logger it writes through.
//
// The runtime logs through a logger of its own rather than the platform's, so
// that it can log before the platform is up. Both share one configuration, so
// both are filtered alike and written to the same place.

#include "core/platform.h"

extern const YLogModule kAppLogModule;
extern YLogger g_app_logger;

#define APP_ERROR(...) \
  Y_LOG(&g_app_logger, &kAppLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define APP_WARN(...) \
  Y_LOG(&g_app_logger, &kAppLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define APP_INFO(...) \
  Y_LOG(&g_app_logger, &kAppLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define APP_DEBUG(...) \
  Y_LOG(&g_app_logger, &kAppLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif  // YAX86_SDL_APP_LOG_H
