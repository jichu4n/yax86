#include "machine_config.h"

#include <stdio.h>

#include "core/platform.h"
#include "disk_image.h"
#include "ff.h"

enum {
  // The most pairs a line may have.
  kMaxPairs = 8,
};

static const char kMachinesFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";
static const char kSettingsFolder[] = "yax86";
static const char kSettingsPath[] = "yax86/yax86.cfg";

static const char* const kDriveKeys[kMachineDrives] = {
    [kMachineDriveA] = "drive_a",
    [kMachineDriveB] = "drive_b",
    [kMachineDriveC] = "drive_c",
    [kMachineDriveD] = "drive_d",
};

static char Upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool SameText(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

// Copy a name in capitals, cut to kMachineNameLength.
static void CopyName(
    const char* from, uint16_t length, char name[kMachineNameLength + 1]) {
  if (length > kMachineNameLength) {
    length = kMachineNameLength;
  }
  for (uint16_t i = 0; i < length; ++i) {
    name[i] = Upper(from[i]);
  }
  name[length] = '\0';
}

// A machine's name, from its file's path: the name of the folder the file is
// in, or the file's own without a folder.
static void NameOf(const char* path, char name[kMachineNameLength + 1]) {
  const char* folder = path;
  const char* file = path;
  for (const char* c = path; *c != '\0'; ++c) {
    if (*c == '/') {
      folder = file;
      file = c + 1;
    }
  }
  if (file == path) {
    const char* end = file;
    while (*end != '\0' && *end != '.') {
      ++end;
    }
    CopyName(file, (uint16_t)(end - file), name);
    return;
  }
  CopyName(folder, (uint16_t)(file - 1 - folder), name);
}

static bool NamesInOrder(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return (unsigned char)*a <= (unsigned char)*b;
}

// Add a machine where it belongs, keeping the list sorted by name.
static void Insert(MachineList* list, const MachineEntry* entry) {
  if (list->count == kMachineListMax) {
    list->truncated = true;
    return;
  }
  uint8_t i = list->count;
  while (i > 0 && !NamesInOrder(list->machines[i - 1].name, entry->name)) {
    list->machines[i] = list->machines[i - 1];
    --i;
  }
  list->machines[i] = *entry;
  ++list->count;
}

uint8_t MachineListFind(MachineList* list, const char* path) {
  for (uint8_t i = 0; i < list->count; ++i) {
    if (FileListSamePath(list->machines[i].path, path)) {
      return i;
    }
  }
  if (list->count == kMachineListMax) {
    return kMachineListMax;
  }
  MachineEntry* entry = &list->machines[list->count];
  NameOf(path, entry->name);
  FileListCopyPath(path, entry->path);
  return list->count++;
}

void MachineListRead(MachineList* list) {
  list->count = 0;
  list->truncated = false;
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  DIR folder;
  if (f_opendir(&folder, kMachinesFolder) != FR_OK) {
    return;
  }
  FILINFO info;
  while (f_readdir(&folder, &info) == FR_OK && info.fname[0] != '\0') {
    if (!(info.fattrib & AM_DIR) || (info.fattrib & (AM_HID | AM_SYS)) ||
        info.fname[0] == '.') {
      continue;
    }
    MachineEntry entry;
    snprintf(
        entry.path, sizeof(entry.path), "%s/%s/%s", kMachinesFolder, info.fname,
        kConfigFile);
    FILINFO config;
    if (f_stat(entry.path, &config) != FR_OK || (config.fattrib & AM_DIR)) {
      continue;
    }
    uint16_t length = 0;
    while (info.fname[length] != '\0') {
      ++length;
    }
    CopyName(info.fname, length, entry.name);
    Insert(list, &entry);
  }
  f_closedir(&folder);
}

bool MachineConfigResolvePath(
    const char* folder, const char* path, char resolved[kFileListMaxPath + 1]) {
  uint16_t length = 0;
  if (path[0] == '/') {
    ++path;
  } else {
    for (const char* c = folder; *c != '\0'; ++c) {
      if (length == kFileListMaxPath) {
        return false;
      }
      resolved[length++] = *c;
    }
  }
  while (*path != '\0') {
    const char* segment = path;
    uint16_t segment_length = 0;
    while (path[segment_length] != '\0' && path[segment_length] != '/') {
      ++segment_length;
    }
    path += segment_length;
    if (*path == '/') {
      ++path;
    }
    if (segment_length == 0 || (segment_length == 1 && segment[0] == '.')) {
      continue;
    }
    if (segment_length == 2 && segment[0] == '.' && segment[1] == '.') {
      if (length == 0) {
        return false;
      }
      while (length > 0 && resolved[length - 1] != '/') {
        --length;
      }
      if (length > 0) {
        // The slash before the segment taken off.
        --length;
      }
      continue;
    }
    if (length > 0) {
      if (length == kFileListMaxPath) {
        return false;
      }
      resolved[length++] = '/';
    }
    for (uint16_t i = 0; i < segment_length; ++i) {
      if (length == kFileListMaxPath) {
        return false;
      }
      resolved[length++] = segment[i];
    }
  }
  resolved[length] = '\0';
  return true;
}

// What a file's pairs are handed to, one at a time. Returns false, with the
// problem described, at one that is not understood.
typedef bool (*TakePair)(
    const char* key, const char* value, void* context,
    char problem[kMachineProblemLength + 1]);

typedef enum ReadResult {
  kReadOk = 0,
  kReadMissing,
  kReadProblem,
} ReadResult;

static void Describe(
    char problem[kMachineProblemLength + 1], const char* text) {
  snprintf(problem, kMachineProblemLength + 1, "%s", text);
}

// Take in one line's pairs, a # line being a comment.
static bool ReadLine(
    char* line, uint16_t line_number, TakePair take, void* context,
    char problem[kMachineProblemLength + 1]) {
  if (line[0] == '#') {
    return true;
  }
  YLogfmtPair pairs[kMaxPairs];
  uint8_t num_pairs = 0;
  if (!YLogfmtParse(line, pairs, kMaxPairs, &num_pairs)) {
    snprintf(
        problem, kMachineProblemLength + 1, "line %u is not key=value pairs",
        line_number);
    return false;
  }
  for (uint8_t i = 0; i < num_pairs; ++i) {
    char what[kMachineProblemLength + 1];
    if (!take(pairs[i].key, pairs[i].value, context, what)) {
      // "line 65535: " leaves room for 48 characters of what.
      snprintf(
          problem, kMachineProblemLength + 1, "line %u: %.48s", line_number,
          what);
      return false;
    }
  }
  return true;
}

// Read a configuration file at path, handing each pair to take.
static ReadResult ReadFile(
    const char* path, TakePair take, void* context,
    char problem[kMachineProblemLength + 1]) {
  // Read whole, so that lines can be cut up in place; static, as it is more
  // than a core's stack should hold.
  static char text[kMachineConfigMaxBytes + 1];
  problem[0] = '\0';
  if (DiskImageMountCard() != kDiskImageOpen) {
    Describe(problem, "no SD card to read");
    return kReadMissing;
  }
  FIL file;
  const FRESULT opened = f_open(&file, path, FA_READ);
  if (opened == FR_NO_FILE || opened == FR_NO_PATH) {
    Describe(problem, "no such file");
    return kReadMissing;
  }
  if (opened != FR_OK) {
    Describe(problem, "could not open the file");
    return kReadProblem;
  }
  UINT read = 0;
  const FRESULT result = f_read(&file, text, kMachineConfigMaxBytes, &read);
  const bool whole = f_eof(&file);
  f_close(&file);
  if (result != FR_OK) {
    Describe(problem, "could not read the file");
    return kReadProblem;
  }
  if (!whole) {
    snprintf(
        problem, kMachineProblemLength + 1, "longer than %u bytes",
        (unsigned)kMachineConfigMaxBytes);
    return kReadProblem;
  }
  text[read] = '\0';
  char* line = text;
  uint16_t line_number = 1;
  while (*line != '\0') {
    char* end = line;
    while (*end != '\0' && *end != '\n') {
      ++end;
    }
    const bool last = *end == '\0';
    *end = '\0';
    if (end > line && end[-1] == '\r') {
      end[-1] = '\0';
    }
    if (!ReadLine(line, line_number, take, context, problem)) {
      return kReadProblem;
    }
    if (last) {
      break;
    }
    line = end + 1;
    ++line_number;
  }
  return kReadOk;
}

// A machine's file being read, and the folder its paths are relative to.
typedef struct MachineReading {
  MachineConfig* config;
  char folder[kFileListMaxPath + 1];
} MachineReading;

static bool TakeMachinePair(
    const char* key, const char* value, void* context,
    char problem[kMachineProblemLength + 1]) {
  MachineReading* reading = context;
  MachineConfig* config = reading->config;
  uint8_t drive = 0;
  while (drive < kMachineDrives && !SameText(key, kDriveKeys[drive])) {
    ++drive;
  }
  if (drive == kMachineDrives) {
    snprintf(problem, kMachineProblemLength + 1, "no setting %s", key);
    return false;
  }
  if (value[0] == '\0') {
    config->drives[drive][0] = '\0';
  } else if (!MachineConfigResolvePath(
                 reading->folder, value, config->drives[drive])) {
    snprintf(problem, kMachineProblemLength + 1, "%s is not a path here", key);
    config->drives[drive][0] = '\0';
    return false;
  }
  return true;
}

bool MachineConfigRead(
    const char* path, MachineConfig* config,
    char problem[kMachineProblemLength + 1]) {
  NameOf(path, config->machine.name);
  FileListCopyPath(path, config->machine.path);
  for (uint8_t drive = 0; drive < kMachineDrives; ++drive) {
    config->drives[drive][0] = '\0';
  }
  MachineReading reading = {.config = config};
  FileListFolderOf(path, reading.folder);
  return ReadFile(path, TakeMachinePair, &reading, problem) == kReadOk;
}

static bool TakeSettingsPair(
    const char* key, const char* value, void* context,
    char problem[kMachineProblemLength + 1]) {
  MachineSettings* settings = context;
  if (SameText(key, "boot_default")) {
    if (value[0] == '\0') {
      settings->boot_default[0] = '\0';
    } else if (!MachineConfigResolvePath(
                   kSettingsFolder, value, settings->boot_default)) {
      settings->boot_default[0] = '\0';
      Describe(problem, "boot_default is not a path here");
      return false;
    }
    return true;
  }
  if (SameText(key, "boot_delay")) {
    uint64_t seconds = 0;
    if (!YLogfmtParseUInt(value, &seconds) || seconds > kMachineMaxBootDelay) {
      snprintf(
          problem, kMachineProblemLength + 1, "boot_delay is not 0 to %u",
          (unsigned)kMachineMaxBootDelay);
      return false;
    }
    settings->boot_delay = (uint8_t)seconds;
    return true;
  }
  snprintf(problem, kMachineProblemLength + 1, "no setting %s", key);
  return false;
}

bool MachineSettingsRead(
    MachineSettings* settings, char problem[kMachineProblemLength + 1]) {
  settings->boot_default[0] = '\0';
  settings->boot_delay = kMachineDefaultBootDelay;
  const ReadResult result =
      ReadFile(kSettingsPath, TakeSettingsPair, settings, problem);
  if (result == kReadMissing) {
    problem[0] = '\0';
  }
  if (result == kReadProblem) {
    return false;
  }
  FILINFO info;
  if (settings->boot_default[0] != '\0' &&
      (f_stat(settings->boot_default, &info) != FR_OK ||
       (info.fattrib & AM_DIR))) {
    snprintf(
        problem, kMachineProblemLength + 1, "no boot_default %.44s",
        settings->boot_default);
    settings->boot_default[0] = '\0';
    return false;
  }
  return true;
}
