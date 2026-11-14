// Logging library.
//
// Provides a YLogger that writes records as logfmt lines - see logfmt.h - and
// hands each line to a caller-provided sink. The core and its hosts write
// through the same logger, so that what any of them prints has one format
// whichever host it runs in.
//
// A line is one of two kinds, named by its first pair:
//
//   kind=log level=warn module=platform tick=1234 cs=0xF000 ip=0xE0A3
//       event=unmapped_port_read port=0x0201
//   kind=reply id=7 cmd=screen status=ok
//
// A log is something that happened, said unprompted: an event name, then
// fields. Logs are filtered at runtime, by module and by severity level,
// before anything is formatted, so a log that is filtered out costs a few
// comparisons. Logging is always compiled in - even on MCU targets the sink
// can write to a debugging serial port.
//
// A reply answers a command a host was sent, carrying the command's id. It is
// never filtered: whoever sent the command is waiting for it.
#ifndef YAX86_UTIL_LOG_H
#define YAX86_UTIL_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Sibling includes are guarded by the target's own include guard rather than
// by YAX86_IMPLEMENTATION, so that this header works both on its own and when
// bundled into a module, in either declaration-only or implementation mode.
#ifndef YAX86_UTIL_COMMON_H
#include "common.h"
#endif  // YAX86_UTIL_COMMON_H
#ifndef YAX86_UTIL_LOGFMT_H
#include "logfmt.h"
#endif  // YAX86_UTIL_LOGFMT_H

// ============================================================================
// Levels and modules
// ============================================================================

// Log severity levels, in decreasing order of severity.
typedef enum YLogLevel {
  // Emulation is likely incorrect, e.g. an invalid opcode or an unmapped
  // memory access.
  Y_LOG_LEVEL_ERROR = 0,
  // Suspicious but handled, e.g. a read from an unmapped I/O port.
  Y_LOG_LEVEL_WARN,
  // Worth knowing in the normal course of things, e.g. how far a boot has
  // got.
  Y_LOG_LEVEL_INFO,
  // Diagnostic detail.
  Y_LOG_LEVEL_DEBUG,
} YLogLevel;

// The level's name, as a line gives it.
Y_PUBLIC_HEADER inline const char* YLogLevelName(YLogLevel level) {
  switch (level) {
    case Y_LOG_LEVEL_ERROR:
      return "error";
    case Y_LOG_LEVEL_WARN:
      return "warn";
    case Y_LOG_LEVEL_INFO:
      return "info";
    case Y_LOG_LEVEL_DEBUG:
      return "debug";
  }
  return "unknown";
}

enum {
  // Maximum length of a line, including the terminating null byte. A longer
  // one is truncated - see logfmt.h.
  Y_LOG_MAX_LINE_LENGTH = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // YLoggerConfig.enabled_modules.
  Y_LOG_MAX_MODULES = 32,
  // Module IDs below this are the core's, and the rest are left for hosts to
  // give modules of their own.
  Y_LOG_FIRST_HOST_MODULE_ID = 16,
};

// Identifies the module a log line originated from.
//
// Each module declares its own YLogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct YLogModule {
  // Bit index used for mask-based filtering. Must be less than
  // Y_LOG_MAX_MODULES.
  uint8_t id;
  // The module's name, as a line gives it: snake_case, e.g. "fdc".
  const char* name;
} YLogModule;

// Returns the filter mask bit for a module.
Y_PUBLIC_HEADER inline uint32_t YLogModuleMask(const YLogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Fields
// ============================================================================

typedef enum YLogFieldType {
  // Marks the end of a list of fields; see Y_LOG().
  Y_LOG_FIELD_END = 0,
  Y_LOG_FIELD_UINT,
  Y_LOG_FIELD_INT,
  Y_LOG_FIELD_HEX,
  Y_LOG_FIELD_BOOL,
  Y_LOG_FIELD_STRING,
} YLogFieldType;

// A key and a typed value, which a line writes as key=value.
typedef struct YLogField {
  // snake_case, e.g. "port".
  const char* key;
  YLogFieldType type;
  // How many hex digits to write at least, for Y_LOG_FIELD_HEX.
  uint8_t num_digits;
  union {
    uint64_t unsigned_value;
    int64_t signed_value;
    bool bool_value;
    const char* string_value;
  } value;
} YLogField;

Y_PUBLIC_HEADER inline YLogField YLogUInt(const char* key, uint64_t value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_UINT;
  field.num_digits = 0;
  field.value.unsigned_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogInt(const char* key, int64_t value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_INT;
  field.num_digits = 0;
  field.value.signed_value = value;
  return field;
}

// A number in hexadecimal, with at least num_digits digits: written 0x0201.
Y_PUBLIC_HEADER inline YLogField YLogHex(
    const char* key, uint64_t value, uint8_t num_digits) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_HEX;
  field.num_digits = num_digits;
  field.value.unsigned_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogBool(const char* key, bool value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_BOOL;
  field.num_digits = 0;
  field.value.bool_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogString(
    const char* key, const char* value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_STRING;
  field.num_digits = 0;
  field.value.string_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogEnd(void) {
  YLogField field;
  field.key = NULL;
  field.type = Y_LOG_FIELD_END;
  field.num_digits = 0;
  field.value.unsigned_value = 0;
  return field;
}

// The fields a call site writes, in the widths the 8086's values come in.
#define Y_LOG_UINT(key, value) YLogUInt((key), (uint64_t)(value))
#define Y_LOG_INT(key, value) YLogInt((key), (int64_t)(value))
#define Y_LOG_HEX8(key, value) YLogHex((key), (uint64_t)(value), 2)
#define Y_LOG_HEX16(key, value) YLogHex((key), (uint64_t)(value), 4)
// A 20-bit physical address.
#define Y_LOG_HEX20(key, value) YLogHex((key), (uint64_t)(value), 5)
#define Y_LOG_HEX32(key, value) YLogHex((key), (uint64_t)(value), 8)
#define Y_LOG_BOOL(key, value) YLogBool((key), (bool)(value))
#define Y_LOG_STRING(key, value) YLogString((key), (value))

// Add a field to a line.
Y_PUBLIC_HEADER inline void YLogAppendField(
    YLogfmtLine* line, const YLogField* field) {
  switch (field->type) {
    case Y_LOG_FIELD_UINT:
      YLogfmtLineAddUInt(line, field->key, field->value.unsigned_value);
      break;
    case Y_LOG_FIELD_INT:
      YLogfmtLineAddInt(line, field->key, field->value.signed_value);
      break;
    case Y_LOG_FIELD_HEX:
      YLogfmtLineAddHex(
          line, field->key, field->value.unsigned_value, field->num_digits);
      break;
    case Y_LOG_FIELD_BOOL:
      YLogfmtLineAddBool(line, field->key, field->value.bool_value);
      break;
    case Y_LOG_FIELD_STRING:
      YLogfmtLineAddString(line, field->key, field->value.string_value);
      break;
    case Y_LOG_FIELD_END:
      break;
  }
}

// Add each field in args to a line, up to the YLogEnd() that ends them.
Y_PUBLIC_HEADER inline void YLogAppendFields(YLogfmtLine* line, va_list args) {
  for (;;) {
    const YLogField field = va_arg(args, YLogField);
    if (field.type == Y_LOG_FIELD_END) {
      return;
    }
    YLogAppendField(line, &field);
  }
}

// ============================================================================
// Logger
// ============================================================================

typedef enum YLogKind {
  Y_LOG_KIND_LOG = 0,
  Y_LOG_KIND_REPLY,
} YLogKind;

// Caller-provided runtime configuration for a logger.
typedef struct YLoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a finished line. The line is null terminated and has
  // no trailing newline. The module and level are those of a log, for a sink
  // that wants to route or colour by them; a reply has neither, and is passed
  // NULL and Y_LOG_LEVEL_INFO.
  void (*write_line)(
      void* context, YLogKind kind, const YLogModule* module, YLogLevel level,
      const char* line, size_t length);

  // Bit mask of enabled modules, indexed by YLogModule.id.
  uint32_t enabled_modules;
  // Maximum severity level to emit. Logs with a level greater than this are
  // suppressed.
  YLogLevel min_level;
} YLoggerConfig;

// Where a logger finds out when and where in the guest a log happened: the
// machine's tick count, and the instruction the CPU is running. Set by
// whoever owns the logger - the platform, for the logger it shares with its
// modules - rather than by the host, which would only wire it to the same
// two things.
typedef struct YLoggerSource {
  void* context;
  // The current tick count, or NULL to leave tick out of each line.
  uint64_t (*get_tick)(void* context);
  // The CS and IP of the instruction running, or NULL to leave them out.
  // Returns false to leave them out of one line.
  bool (*get_location)(void* context, uint16_t* cs, uint16_t* ip);
} YLoggerSource;

// State of a logger.
typedef struct YLogger {
  // Pointer to caller-provided runtime configuration.
  YLoggerConfig* config;
  YLoggerSource source;
  // Scratch buffer a line is built in. A logger is therefore not reentrant:
  // a write_line callback must never log.
  char buffer[Y_LOG_MAX_LINE_LENGTH];
} YLogger;

// Initialize a logger with the provided configuration, and no source.
Y_PUBLIC_HEADER inline void YLoggerInit(
    YLogger* logger, YLoggerConfig* config) {
  logger->config = config;
  logger->source.context = NULL;
  logger->source.get_tick = NULL;
  logger->source.get_location = NULL;
  logger->buffer[0] = '\0';
}

// Set where the logger finds the tick and the location for each log.
Y_PUBLIC_HEADER inline void YLoggerSetSource(
    YLogger* logger, const YLoggerSource* source) {
  logger->source = *source;
}

// Whether a log with the given module and level would be emitted. This is
// checked before anything is formatted, so that disabled log statements cost
// only a few comparisons.
Y_PUBLIC_HEADER inline bool YLoggerIsEnabled(
    const YLogger* logger, const YLogModule* module, YLogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_modules & YLogModuleMask(module)) != 0;
}

// Enable a module on a logger.
Y_PUBLIC_HEADER inline void YLoggerEnableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules |= YLogModuleMask(module);
  }
}

// Disable a module on a logger.
Y_PUBLIC_HEADER inline void YLoggerDisableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules &= ~YLogModuleMask(module);
  }
}

// Write a log: an event, then the fields that follow it, ended by YLogEnd().
// Prefer the Y_LOG macro, which skips all of it when the log would be
// suppressed, and ends the fields itself.
Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* event, ...) Y_UNUSED;
Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* event, ...) {
  // Callers normally go through Y_LOG, which has already checked this, but
  // YLoggerWrite is also callable directly.
  if (!YLoggerIsEnabled(logger, module, level)) {
    return;
  }
  YLogfmtLine line;
  YLogfmtLineInit(&line, logger->buffer, Y_LOG_MAX_LINE_LENGTH);
  YLogfmtLineAddString(&line, "kind", "log");
  YLogfmtLineAddString(&line, "level", YLogLevelName(level));
  YLogfmtLineAddString(&line, "module", module->name);
  const YLoggerSource* source = &logger->source;
  if (source->get_tick != NULL) {
    YLogfmtLineAddUInt(&line, "tick", source->get_tick(source->context));
  }
  uint16_t cs = 0;
  uint16_t ip = 0;
  if (source->get_location != NULL &&
      source->get_location(source->context, &cs, &ip)) {
    YLogfmtLineAddHex(&line, "cs", cs, 4);
    YLogfmtLineAddHex(&line, "ip", ip, 4);
  }
  YLogfmtLineAddString(&line, "event", event);
  va_list args;
  va_start(args, event);
  YLogAppendFields(&line, args);
  va_end(args);
  const size_t length = YLogfmtLineFinish(&line);
  logger->config->write_line(
      logger->config->context, Y_LOG_KIND_LOG, module, level, logger->buffer,
      length);
}

// Write a reply to the command with the given id: the command's name, then
// the fields that follow it, ended by YLogEnd(). Never filtered. Prefer the
// Y_LOG_REPLY macro, which ends the fields itself.
Y_PUBLIC_HEADER void YLoggerWriteReply(
    YLogger* logger, uint32_t id, const char* command, ...) Y_UNUSED;
Y_PUBLIC_HEADER void YLoggerWriteReply(
    YLogger* logger, uint32_t id, const char* command, ...) {
  if (logger == NULL || logger->config == NULL ||
      logger->config->write_line == NULL) {
    return;
  }
  YLogfmtLine line;
  YLogfmtLineInit(&line, logger->buffer, Y_LOG_MAX_LINE_LENGTH);
  YLogfmtLineAddString(&line, "kind", "reply");
  YLogfmtLineAddUInt(&line, "id", id);
  YLogfmtLineAddString(&line, "cmd", command);
  va_list args;
  va_start(args, command);
  YLogAppendFields(&line, args);
  va_end(args);
  const size_t length = YLogfmtLineFinish(&line);
  logger->config->write_line(
      logger->config->context, Y_LOG_KIND_REPLY, NULL, Y_LOG_LEVEL_INFO,
      logger->buffer, length);
}

// Write a log, skipping everything - the fields included - if it would be
// suppressed: Y_LOG(logger, module, level, "event_name", fields...), where
// each field is one of the Y_LOG_UINT() family above.
//
// The event is the first of the variadic arguments rather than a parameter of
// its own, so that a log with no fields is still a valid C99 macro call.
#define Y_LOG(logger, module, level, ...)                                \
  do {                                                                   \
    if (YLoggerIsEnabled((logger), (module), (level))) {                 \
      YLoggerWrite((logger), (module), (level), __VA_ARGS__, YLogEnd()); \
    }                                                                    \
  } while (0)

// Write a reply: Y_LOG_REPLY(logger, id, "command", fields...).
#define Y_LOG_REPLY(logger, id, ...) \
  YLoggerWriteReply((logger), (id), __VA_ARGS__, YLogEnd())

#endif  // YAX86_UTIL_LOG_H
