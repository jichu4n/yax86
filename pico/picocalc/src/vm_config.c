#include "vm_config.h"

#include <stdio.h>

#include "config_file.h"
#include "core/platform.h"
#include "disk_image.h"
#include "ff.h"

static const char kVMsFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";
static const char kSettingsFolder[] = "yax86";
static const char kSettingsPath[] = "yax86/yax86.cfg";
static const char kBootDefaultKey[] = "boot_default";

static const char* const kDriveKeys[kVMDrives] = {
    [kVMDriveA] = "drive_a",
    [kVMDriveB] = "drive_b",
    [kVMDriveC] = "drive_c",
    [kVMDriveD] = "drive_d",
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

// Copy a name in capitals, cut to kVMNameLength.
static void CopyName(
    const char* from, uint16_t length, char name[kVMNameLength + 1]) {
  if (length > kVMNameLength) {
    length = kVMNameLength;
  }
  for (uint16_t i = 0; i < length; ++i) {
    name[i] = Upper(from[i]);
  }
  name[length] = '\0';
}

// A VM's name, from its file's path: the name of the folder the file is
// in, or the file's own without a folder.
static void NameOf(const char* path, char name[kVMNameLength + 1]) {
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

// Add a VM where it belongs, keeping the list sorted by name.
static void Insert(VMList* list, const VMEntry* entry) {
  if (list->count == kVMListMax) {
    list->truncated = true;
    return;
  }
  uint8_t i = list->count;
  while (i > 0 && !NamesInOrder(list->vms[i - 1].name, entry->name)) {
    list->vms[i] = list->vms[i - 1];
    --i;
  }
  list->vms[i] = *entry;
  ++list->count;
}

uint8_t VMListFind(VMList* list, const char* path) {
  for (uint8_t i = 0; i < list->count; ++i) {
    if (FileListSamePath(list->vms[i].path, path)) {
      return i;
    }
  }
  if (list->count == kVMListMax) {
    return kVMListMax;
  }
  VMEntry* entry = &list->vms[list->count];
  NameOf(path, entry->name);
  FileListCopyPath(path, entry->path);
  return list->count++;
}

void VMListRead(VMList* list) {
  list->count = 0;
  list->truncated = false;
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  DIR folder;
  if (f_opendir(&folder, kVMsFolder) != FR_OK) {
    return;
  }
  FILINFO info;
  while (f_readdir(&folder, &info) == FR_OK && info.fname[0] != '\0') {
    if (!(info.fattrib & AM_DIR) || (info.fattrib & (AM_HID | AM_SYS)) ||
        info.fname[0] == '.') {
      continue;
    }
    VMEntry entry;
    snprintf(
        entry.path, sizeof(entry.path), "%s/%s/%s", kVMsFolder, info.fname,
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

bool VMConfigResolvePath(
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

// A VM's file being read, and the folder its paths are relative to.
typedef struct VMReading {
  VMConfig* config;
  char folder[kFileListMaxPath + 1];
} VMReading;

static bool TakeVMPair(
    const char* key, const char* value, void* context,
    char problem[kVMProblemLength + 1]) {
  VMReading* reading = context;
  VMConfig* config = reading->config;
  uint8_t drive = 0;
  while (drive < kVMDrives && !SameText(key, kDriveKeys[drive])) {
    ++drive;
  }
  if (drive == kVMDrives) {
    snprintf(problem, kVMProblemLength + 1, "no setting %s", key);
    return false;
  }
  if (value[0] == '\0') {
    config->drives[drive][0] = '\0';
  } else if (!VMConfigResolvePath(
                 reading->folder, value, config->drives[drive])) {
    snprintf(problem, kVMProblemLength + 1, "%s is not a path here", key);
    config->drives[drive][0] = '\0';
    return false;
  }
  return true;
}

bool VMConfigRead(
    const char* path, VMConfig* config, char problem[kVMProblemLength + 1]) {
  NameOf(path, config->entry.name);
  FileListCopyPath(path, config->entry.path);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    config->drives[drive][0] = '\0';
  }
  VMReading reading = {.config = config};
  FileListFolderOf(path, reading.folder);
  return ConfigFileRead(path, TakeVMPair, &reading, problem) == kConfigFileOk;
}

static bool TakeSettingsPair(
    const char* key, const char* value, void* context,
    char problem[kVMProblemLength + 1]) {
  VMSettings* settings = context;
  if (SameText(key, kBootDefaultKey)) {
    if (value[0] == '\0') {
      settings->boot_default[0] = '\0';
    } else if (!VMConfigResolvePath(
                   kSettingsFolder, value, settings->boot_default)) {
      settings->boot_default[0] = '\0';
      snprintf(
          problem, kVMProblemLength + 1, "boot_default is not a path here");
      return false;
    }
    return true;
  }
  if (SameText(key, "boot_delay")) {
    uint64_t seconds = 0;
    if (!YLogfmtParseUInt(value, &seconds) || seconds > kVMMaxBootDelay) {
      snprintf(
          problem, kVMProblemLength + 1, "boot_delay is not 0 to %u",
          (unsigned)kVMMaxBootDelay);
      return false;
    }
    settings->boot_delay = (uint8_t)seconds;
    return true;
  }
  snprintf(problem, kVMProblemLength + 1, "no setting %s", key);
  return false;
}

bool VMSettingsRead(VMSettings* settings, char problem[kVMProblemLength + 1]) {
  settings->boot_default[0] = '\0';
  settings->boot_delay = kVMDefaultBootDelay;
  const ConfigFileResult result =
      ConfigFileRead(kSettingsPath, TakeSettingsPair, settings, problem);
  if (result == kConfigFileMissing) {
    problem[0] = '\0';
  }
  if (result == kConfigFileProblem) {
    return false;
  }
  FILINFO info;
  if (settings->boot_default[0] != '\0' &&
      (f_stat(settings->boot_default, &info) != FR_OK ||
       (info.fattrib & AM_DIR))) {
    snprintf(
        problem, kVMProblemLength + 1, "no boot_default %.44s",
        settings->boot_default);
    settings->boot_default[0] = '\0';
    return false;
  }
  return true;
}

static char Lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool PutChar(char out[kFileListMaxPath + 2], uint16_t* length, char c) {
  if (*length == kFileListMaxPath + 1) {
    return false;
  }
  out[(*length)++] = c;
  return true;
}

bool VMConfigRelativePath(
    const char* folder, const char* path, char out[kFileListMaxPath + 2]) {
  // The folders the two have in common, from the root: path's last segment
  // is its file, never one of them.
  const char* f = folder;
  const char* p = path;
  uint8_t common = 0;
  while (*f != '\0') {
    uint16_t folder_length = 0;
    while (f[folder_length] != '\0' && f[folder_length] != '/') {
      ++folder_length;
    }
    uint16_t path_length = 0;
    while (p[path_length] != '\0' && p[path_length] != '/') {
      ++path_length;
    }
    if (p[path_length] != '/' || folder_length != path_length) {
      break;
    }
    uint16_t i = 0;
    while (i < folder_length && Upper(f[i]) == Upper(p[i])) {
      ++i;
    }
    if (i < folder_length) {
      break;
    }
    ++common;
    f += folder_length;
    if (*f == '/') {
      ++f;
    }
    p += path_length + 1;
  }
  uint16_t length = 0;
  if (common == 0) {
    // Nothing in common: from the root.
    p = path;
    PutChar(out, &length, '/');
  } else if (*f != '\0') {
    // Up out of each of folder's segments left.
    for (const char* c = f;; ++c) {
      if (*c == '/' || *c == '\0') {
        if (!PutChar(out, &length, '.') || !PutChar(out, &length, '.') ||
            !PutChar(out, &length, '/')) {
          return false;
        }
      }
      if (*c == '\0') {
        break;
      }
    }
  }
  for (; *p != '\0'; ++p) {
    if (!PutChar(out, &length, Lower(*p))) {
      return false;
    }
  }
  out[length] = '\0';
  return true;
}

bool VMConfigSaveDrive(
    const VMConfig* config, uint8_t drive, const char* path,
    char problem[kVMProblemLength + 1]) {
  char value[kFileListMaxPath + 2] = "";
  if (path != NULL && path[0] != '\0') {
    char folder[kFileListMaxPath + 1];
    FileListFolderOf(config->entry.path, folder);
    if (!VMConfigRelativePath(folder, path, value)) {
      snprintf(problem, kVMProblemLength + 1, "path too long");
      return false;
    }
  }
  const ConfigFileSetting setting = {.key = kDriveKeys[drive], .value = value};
  return ConfigFileUpdate(config->entry.path, &setting, 1, problem);
}

bool VMSettingsSaveBootDefault(
    const char* path, char problem[kVMProblemLength + 1]) {
  char value[kFileListMaxPath + 2] = "";
  if (path[0] != '\0' && !VMConfigRelativePath(kSettingsFolder, path, value)) {
    snprintf(problem, kVMProblemLength + 1, "path too long");
    return false;
  }
  const ConfigFileSetting setting = {.key = kBootDefaultKey, .value = value};
  return ConfigFileUpdate(kSettingsPath, &setting, 1, problem);
}
