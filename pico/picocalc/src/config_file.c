#include "config_file.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "disk_image.h"
#include "ff.h"
#include "fs.h"

enum {
  // The most pairs a line may have.
  kMaxPairs = 8,
};

static const char kNewExtension[] = "new";

static void Describe(
    char problem[kConfigFileProblemLength + 1], const char* text) {
  snprintf(problem, kConfigFileProblemLength + 1, "%s", text);
}

// ============================================================================
// Reading
// ============================================================================

// A file's text, read whole so that lines can be cut up in place; static, as
// it is more than a core's stack should hold.
static char g_text[kConfigFileMaxBytes + 1];

// Read the file at path whole into g_text, ended with a NUL.
static ConfigFileResult LoadFile(
    const char* path, char problem[kConfigFileProblemLength + 1]) {
  problem[0] = '\0';
  if (DiskImageMountCard() != kDiskImageOpen) {
    Describe(problem, "no SD card to read");
    return kConfigFileMissing;
  }
  FIL file;
  const FRESULT opened = f_open(&file, path, FA_READ);
  if (opened == FR_NO_FILE || opened == FR_NO_PATH) {
    Describe(problem, "no such file");
    return kConfigFileMissing;
  }
  if (opened != FR_OK) {
    Describe(problem, "could not open the file");
    return kConfigFileProblem;
  }
  UINT read = 0;
  const FRESULT result = f_read(&file, g_text, kConfigFileMaxBytes, &read);
  const bool is_whole = f_eof(&file);
  f_close(&file);
  if (result != FR_OK) {
    Describe(problem, "could not read the file");
    return kConfigFileProblem;
  }
  if (!is_whole) {
    snprintf(
        problem, kConfigFileProblemLength + 1, "longer than %u bytes",
        (unsigned)kConfigFileMaxBytes);
    return kConfigFileProblem;
  }
  g_text[read] = '\0';
  return kConfigFileOk;
}

// Take in one line's pairs, a # line being a comment.
static bool ReadLine(
    char* line, uint16_t line_number, ConfigFileTakePair take, void* context,
    char problem[kConfigFileProblemLength + 1]) {
  if (line[0] == '#') {
    return true;
  }
  YLogfmtPair pairs[kMaxPairs];
  uint8_t num_pairs = 0;
  if (!YLogfmtParse(line, pairs, kMaxPairs, &num_pairs)) {
    snprintf(
        problem, kConfigFileProblemLength + 1, "line %u is not key=value pairs",
        line_number);
    return false;
  }
  for (uint8_t i = 0; i < num_pairs; ++i) {
    char what[kConfigFileProblemLength + 1];
    if (!take(pairs[i].key, pairs[i].value, context, what)) {
      // "line 65535: " leaves room for 48 characters of what.
      snprintf(
          problem, kConfigFileProblemLength + 1, "line %u: %.48s", line_number,
          what);
      return false;
    }
  }
  return true;
}

ConfigFileResult ConfigFileRead(
    const char* path, ConfigFileTakePair take, void* context,
    char problem[kConfigFileProblemLength + 1]) {
  const ConfigFileResult loaded = LoadFile(path, problem);
  if (loaded != kConfigFileOk) {
    return loaded;
  }
  char* line = g_text;
  uint16_t line_number = 1;
  while (*line != '\0') {
    char* end = line;
    while (*end != '\0' && *end != '\n') {
      ++end;
    }
    const bool is_last = *end == '\0';
    *end = '\0';
    if (end > line && end[-1] == '\r') {
      end[-1] = '\0';
    }
    if (!ReadLine(line, line_number, take, context, problem)) {
      return kConfigFileProblem;
    }
    if (is_last) {
      break;
    }
    line = end + 1;
    ++line_number;
  }
  return kConfigFileOk;
}

// ============================================================================
// Updating
// ============================================================================

// A file being written: the text so far, and whether it still fits.
typedef struct Writer {
  char text[kConfigFileMaxBytes + 1];
  uint16_t length;
  bool does_fit;
} Writer;

static void Write(Writer* writer, const char* from, const char* end) {
  for (const char* c = from; c < end; ++c) {
    if (writer->length == kConfigFileMaxBytes) {
      writer->does_fit = false;
      return;
    }
    writer->text[writer->length++] = *c;
  }
}

static void WriteText(Writer* writer, const char* text) {
  const char* end = text;
  while (*end != '\0') {
    ++end;
  }
  Write(writer, text, end);
}

// A setting as a pair, quoted and escaped as logfmt needs, which is how
// YLogfmtParse() reads it back.
static void WritePair(Writer* writer, const ConfigFileSetting* setting) {
  static char pair[kConfigFileMaxBytes + Y_LOGFMT_RESERVED_LENGTH];
  YLogfmtLine line;
  YLogfmtLineInit(&line, pair, sizeof(pair));
  YLogfmtLineAddString(&line, setting->key, setting->value);
  if (line.is_truncated) {
    writer->does_fit = false;
    return;
  }
  Write(writer, pair, pair + line.length);
}

static bool IsSpace(char c) { return c == ' ' || c == '\t'; }

// The next pair in a line, from *c up to end: where it starts, where its key
// ends, and where it ends, with *c left past it. A quoted value runs to its
// closing quote. Returns false at the end of the line.
static bool NextPair(
    const char** c, const char* end, const char** pair_start,
    const char** key_end, const char** pair_end) {
  const char* p = *c;
  while (p < end && IsSpace(*p)) {
    ++p;
  }
  if (p == end) {
    return false;
  }
  *pair_start = p;
  while (p < end && *p != '=' && !IsSpace(*p)) {
    ++p;
  }
  *key_end = p;
  if (p < end && *p == '=') {
    ++p;
    if (p < end && *p == '"') {
      ++p;
      while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
          ++p;
        }
        ++p;
      }
      if (p < end) {
        ++p;
      }
    } else {
      while (p < end && !IsSpace(*p)) {
        ++p;
      }
    }
  }
  *pair_end = p;
  *c = p;
  return true;
}

// The setting whose key is from start to end, or num_settings if there is none.
static uint8_t SettingFor(
    const ConfigFileSetting* settings, uint8_t num_settings, const char* start,
    const char* end) {
  for (uint8_t i = 0; i < num_settings; ++i) {
    const char* k = settings[i].key;
    const char* t = start;
    while (*k != '\0' && t < end && *k == *t) {
      ++k;
      ++t;
    }
    if (*k == '\0' && t == end) {
      return i;
    }
  }
  return num_settings;
}

// path with its extension, if it has one, replaced by .new.
static bool NewPath(const char* path, char new_path[kFSMaxPath + 1]) {
  const char* slash = strrchr(path, '/');
  const char* dot = strrchr(path, '.');
  const int stem = dot != NULL && (slash == NULL || dot > slash)
                       ? (int)(dot - path)
                       : (int)strlen(path);
  const int length =
      snprintf(new_path, kFSMaxPath + 1, "%.*s.%s", stem, path, kNewExtension);
  return length >= 0 && length <= kFSMaxPath;
}

// Write writer's text to path, through a file beside it at new_path.
static bool Replace(
    const char* path, const char* new_path, const Writer* writer,
    char problem[kConfigFileProblemLength + 1]) {
  FIL file;
  if (f_open(&file, new_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
    Describe(problem, "could not create the new file");
    return false;
  }
  UINT written = 0;
  const FRESULT wrote = f_write(&file, writer->text, writer->length, &written);
  const FRESULT closed = f_close(&file);
  if (wrote != FR_OK || closed != FR_OK || written != writer->length) {
    f_unlink(new_path);
    Describe(problem, "could not write the new file");
    return false;
  }
  const FRESULT removed = f_unlink(path);
  if (removed != FR_OK && removed != FR_NO_FILE) {
    f_unlink(new_path);
    Describe(problem, "could not remove the old file");
    return false;
  }
  if (f_rename(new_path, path) != FR_OK) {
    Describe(problem, "could not rename the new file");
    return false;
  }
  return true;
}

bool ConfigFileUpdate(
    const char* path, const ConfigFileSetting* settings, uint8_t num_settings,
    char problem[kConfigFileProblemLength + 1]) {
  static Writer writer;
  writer.length = 0;
  writer.does_fit = true;
  if (num_settings > kConfigFileMaxSettings) {
    Describe(problem, "too many settings at once");
    return false;
  }
  char new_path[kFSMaxPath + 1];
  if (!NewPath(path, new_path)) {
    Describe(problem, "path too long");
    return false;
  }
  if (DiskImageMountCard() != kDiskImageOpen) {
    Describe(problem, "no SD card to write");
    return false;
  }
  const ConfigFileResult loaded = LoadFile(path, problem);
  if (loaded == kConfigFileProblem) {
    return false;
  }
  if (loaded == kConfigFileMissing) {
    g_text[0] = '\0';
  }
  problem[0] = '\0';

  // Which settings have a pair already, each bit a setting.
  uint32_t written = 0;
  bool has_crlf = false;
  const char* line = g_text;
  while (*line != '\0') {
    const char* end = line;
    while (*end != '\0' && *end != '\n') {
      ++end;
    }
    const char* content_end = end;
    if (content_end > line && content_end[-1] == '\r') {
      --content_end;
      has_crlf = true;
    }
    const char* first = line;
    while (first < content_end && IsSpace(*first)) {
      ++first;
    }
    const char* c = line;
    // Where the line starts in the new file, and whether a pair was removed
    // from it, which can leave nothing on it.
    const uint16_t line_start = writer.length;
    bool was_removed = false;
    if (first < content_end && *first != '#') {
      const char* pair_start;
      const char* key_end;
      const char* pair_end;
      const char* copied = line;
      while (NextPair(&c, content_end, &pair_start, &key_end, &pair_end)) {
        const uint8_t i =
            SettingFor(settings, num_settings, pair_start, key_end);
        if (i < num_settings) {
          Write(&writer, copied, pair_start);
          if (settings[i].value != NULL) {
            WritePair(&writer, &settings[i]);
          } else {
            was_removed = true;
          }
          written |= (uint32_t)1 << i;
          copied = pair_end;
        }
      }
      c = copied;
    }
    Write(&writer, c, content_end);
    bool is_blank = was_removed;
    for (uint16_t i = line_start; is_blank && i < writer.length; ++i) {
      is_blank = IsSpace(writer.text[i]);
    }
    if (is_blank) {
      // A line its pairs were all removed from goes with them.
      writer.length = line_start;
    } else {
      Write(&writer, content_end, *end == '\0' ? end : end + 1);
    }
    line = *end == '\0' ? end : end + 1;
  }
  const char* const newline = has_crlf ? "\r\n" : "\n";
  for (uint8_t i = 0; i < num_settings; ++i) {
    if ((written & ((uint32_t)1 << i)) || settings[i].value == NULL) {
      continue;
    }
    if (writer.length > 0 && writer.text[writer.length - 1] != '\n') {
      WriteText(&writer, newline);
    }
    WritePair(&writer, &settings[i]);
    WriteText(&writer, newline);
  }
  if (!writer.does_fit) {
    snprintf(
        problem, kConfigFileProblemLength + 1, "would be longer than %u bytes",
        (unsigned)kConfigFileMaxBytes);
    return false;
  }
  return Replace(path, new_path, &writer, problem);
}
