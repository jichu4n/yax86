#ifndef YAX86_IMPLEMENTATION
#include <stdarg.h>

#include "../util/snprintf.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void LoggerInit(Logger* logger, LoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

void LoggerWrite(
    Logger* logger, const LogCategory* category, LogLevel level,
    const char* format, ...) {
  // Callers normally go through YAX86_LOG, which has already checked this, but
  // LoggerWrite is also callable directly.
  if (!LoggerIsEnabled(logger, category, level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  int formatted_length =
      VSNPrintF(logger->buffer, kLogMaxLineLength, format, args);
  va_end(args);

  // VSNPrintF returns the length the message would have had, which may exceed
  // the buffer. Report the length actually written instead.
  size_t length = 0;
  if (formatted_length > 0) {
    length = (size_t)formatted_length < kLogMaxLineLength
                 ? (size_t)formatted_length
                 : kLogMaxLineLength - 1;
  }

  uint64_t tick = logger->config->get_tick != NULL
                      ? logger->config->get_tick(logger->config->context)
                      : 0;
  logger->config->write_line(
      logger->config->context, category, level, tick, logger->buffer, length);
}
