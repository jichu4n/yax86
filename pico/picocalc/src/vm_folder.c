#include "vm_folder.h"

#include <stdio.h>

#include "ff.h"

static const char kVMsFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";

enum {
  // How deep in a VM's folder a copy or a delete goes: folders in folders.
  kMaxDepth = 4,
  kBytesPerKB = 1024,
};

// Names DOS gives its devices, which a folder named so cannot be opened as
// from DOS, or from some computers the card goes into.
static const char* const kDeviceNames[] = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

static char Upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static uint16_t Length(const char* text) {
  uint16_t length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  return length;
}

static bool SameName(const char* a, const char* b) {
  while (*a != '\0' && Upper(*a) == Upper(*b)) {
    ++a;
    ++b;
  }
  return *a == *b;
}

// Whether path is a file or folder inside folder, at any depth.
static bool InFolder(const char* path, const char* folder) {
  while (*folder != '\0' && Upper(*folder) == Upper(*path)) {
    ++folder;
    ++path;
  }
  return *folder == '\0' && *path == '/';
}

// path, inside from, as the same file inside to. Returns false if it would not
// fit.
static bool Rebase(
    const char* path, const char* from, const char* to,
    char rebased[kFileListMaxPath + 1]) {
  const int written =
      snprintf(rebased, kFileListMaxPath + 1, "%s%s", to, path + Length(from));
  return written >= 0 && written <= kFileListMaxPath;
}

// Add a name to the end of a path, after a slash, returning the new length,
// or 0 if it would not fit.
static uint16_t Append(
    char path[kFileListMaxPath + 1], uint16_t length, const char* name) {
  const uint16_t name_length = Length(name);
  if (length + 1 + name_length > kFileListMaxPath) {
    return 0;
  }
  path[length] = '/';
  for (uint16_t i = 0; i <= name_length; ++i) {
    path[length + 1 + i] = name[i];
  }
  return (uint16_t)(length + 1 + name_length);
}

// Whether a folder's entry read is one: f_readdir() gives an empty name at
// the end.
static bool IsEntry(const FILINFO* info) { return info->fname[0] != '\0'; }

// Whether an entry is "." or "..", which FatFs does not list, but which are
// skipped to be sure.
static bool IsDots(const FILINFO* info) { return info->fname[0] == '.'; }

// Why the card would not do something to a VM: "Could not copy DOS330: the
// card is full".
static void Describe(
    const char* verb, const VMEntry* vm, FRESULT result,
    char problem[kVMProblemLength + 1]) {
  const char* why = "the card failed";
  switch (result) {
    case FR_DENIED:
      why = "the card is full";
      break;
    case FR_EXIST:
      why = "it is already there";
      break;
    case FR_NO_FILE:
    case FR_NO_PATH:
      why = "it is not there";
      break;
    case FR_WRITE_PROTECTED:
      why = "the card is locked";
      break;
    default:
      break;
  }
  snprintf(
      problem, kVMProblemLength + 1, "Could not %s %s: %s", verb, vm->name,
      why);
}

bool VMFolderIsManaged(const VMEntry* vm) {
  // yax86/vm/NAME/vm.cfg: a folder, in the VMs folder.
  if (!InFolder(vm->path, kVMsFolder)) {
    return false;
  }
  const char* name = vm->path + Length(kVMsFolder) + 1;
  uint16_t length = 0;
  while (name[length] != '\0' && name[length] != '/') {
    ++length;
  }
  return length > 0 && name[length] == '/' &&
         SameName(name + length + 1, kConfigFile);
}

bool VMFolderNameCharacter(char c) {
  c = Upper(c);
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '_';
}

// The folder a VM named name has, from a name typed: in capitals, and checked.
static bool FolderFor(
    const char* name, char folder[kFileListMaxPath + 1],
    char problem[kVMProblemLength + 1]) {
  const uint16_t length = Length(name);
  if (length == 0) {
    snprintf(problem, kVMProblemLength + 1, "Type a name for the VM");
    return false;
  }
  char upper[kVMFolderMaxName + 1];
  for (uint16_t i = 0; i < length; ++i) {
    if (i == kVMFolderMaxName || !VMFolderNameCharacter(name[i])) {
      snprintf(
          problem, kVMProblemLength + 1,
          "A name is up to %u letters, digits, - and _",
          (unsigned)kVMFolderMaxName);
      return false;
    }
    upper[i] = Upper(name[i]);
  }
  upper[length] = '\0';
  for (uint8_t i = 0; i < sizeof(kDeviceNames) / sizeof(kDeviceNames[0]); ++i) {
    if (SameName(upper, kDeviceNames[i])) {
      snprintf(
          problem, kVMProblemLength + 1, "%s is the name of a DOS device",
          upper);
      return false;
    }
  }
  snprintf(folder, kFileListMaxPath + 1, "%s/%s", kVMsFolder, upper);
  return true;
}

static bool Taken(const char* folder, char problem[kVMProblemLength + 1]) {
  FILINFO info;
  if (f_stat(folder, &info) != FR_OK) {
    return false;
  }
  snprintf(
      problem, kVMProblemLength + 1, "%s is already taken",
      folder + Length(kVMsFolder) + 1);
  return true;
}

// A VM's folder and the one it is going to, for a rename or a copy, which the
// walks below add to as they go down: too large to keep two of on the stack.
static char g_from[kFileListMaxPath + 1];
static char g_to[kFileListMaxPath + 1];

// The VM read by Retarget() and VMFolderDescribe(): too large for the stack,
// with ConfigFileUpdate() under it.
static VMConfig g_config;

// Write again the drives of the VM whose file is at path that name a file in
// from, to name it in to. A VM that does not read has what was read of it
// written again, and nothing else; one with nothing in from is not written.
static bool Retarget(
    const char* path, const char* from, const char* to,
    char problem[kVMProblemLength + 1]) {
  char ignored[kVMProblemLength + 1];
  VMConfigRead(path, &g_config, ignored);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    if (!InFolder(g_config.drives[drive], from)) {
      continue;
    }
    char rebased[kFileListMaxPath + 1];
    if (!Rebase(g_config.drives[drive], from, to, rebased)) {
      snprintf(
          problem, kVMProblemLength + 1, "A disk's path would be too long");
      return false;
    }
    if (!VMConfigSaveDrive(&g_config, drive, rebased, problem)) {
      return false;
    }
  }
  return true;
}

void VMFolderDescribe(
    const VMList* list, const VMEntry* vm, VMFolderContents* contents) {
  contents->file_count = 0;
  contents->user_count = 0;
  char folder[kFileListMaxPath + 1];
  FileListFolderOf(vm->path, folder);
  DIR dir;
  if (f_opendir(&dir, folder) == FR_OK) {
    FILINFO info;
    while (f_readdir(&dir, &info) == FR_OK && IsEntry(&info)) {
      if (IsDots(&info) || SameName(info.fname, kConfigFile)) {
        continue;
      }
      if (contents->file_count < kVMFolderNamed) {
        char* name = contents->files[contents->file_count];
        for (uint8_t c = 0; c <= kFileListMaxName; ++c) {
          name[c] = info.fname[c];
        }
      }
      if (contents->file_count < UINT8_MAX) {
        ++contents->file_count;
      }
    }
    f_closedir(&dir);
  }
  for (uint8_t i = 0; i < list->count; ++i) {
    const VMEntry* other = &list->vms[i];
    if (FileListSamePath(other->path, vm->path)) {
      continue;
    }
    char ignored[kVMProblemLength + 1];
    VMConfigRead(other->path, &g_config, ignored);
    for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
      if (InFolder(g_config.drives[drive], folder)) {
        if (contents->user_count < kVMFolderNamed) {
          char* name = contents->users[contents->user_count];
          for (uint8_t c = 0; c <= kVMNameLength; ++c) {
            name[c] = other->name[c];
          }
        }
        ++contents->user_count;
        break;
      }
    }
  }
}

VMFolderResult VMFolderRename(
    const VMList* list, const VMEntry* vm, const char* name,
    char path[kFileListMaxPath + 1], char problem[kVMProblemLength + 1]) {
  // The folder and its new name, and the settings: kept off the stack, which
  // writing a configuration file under them takes much of.
  char* const from = g_from;
  char* const to = g_to;
  static VMSettings settings;
  FileListFolderOf(vm->path, from);
  if (!FolderFor(name, to, problem)) {
    return kVMFolderBadName;
  }
  FileListCopyPath(to, path);
  Append(path, Length(path), kConfigFile);
  // The same name, perhaps in other capitals, which the card keeps as one.
  if (FileListSamePath(from, to)) {
    return kVMFolderDone;
  }
  if (Taken(to, problem)) {
    return kVMFolderBadName;
  }
  // Read before the rename, which leaves boot_default naming no file.
  char ignored[kVMProblemLength + 1];
  VMSettingsRead(&settings, ignored);
  const FRESULT renamed = f_rename(from, to);
  if (renamed != FR_OK) {
    Describe("rename", vm, renamed, problem);
    return kVMFolderFailed;
  }
  if (InFolder(settings.boot_default, from)) {
    char rebased[kFileListMaxPath + 1];
    if (!Rebase(settings.boot_default, from, to, rebased) ||
        !VMSettingsSaveBootDefault(rebased, problem)) {
      return kVMFolderFailed;
    }
  }
  for (uint8_t i = 0; i < list->count; ++i) {
    const char* other = list->vms[i].path;
    if (!Retarget(
            FileListSamePath(other, vm->path) ? path : other, from, to,
            problem)) {
      return kVMFolderFailed;
    }
  }
  return kVMFolderDone;
}

// ============================================================================
// Copying and deleting
// ============================================================================

// Delete what is in the folder at path, and with keep_config, everything but
// a vm.cfg directly in it. path is the caller's, and is added to and put back
// as the walk goes down and up.
static FRESULT DeleteContents(
    char path[kFileListMaxPath + 1], uint16_t length, uint8_t depth,
    bool keep_config) {
  DIR dir;
  FRESULT result = f_opendir(&dir, path);
  FILINFO info;
  while (result == FR_OK && (result = f_readdir(&dir, &info)) == FR_OK &&
         IsEntry(&info)) {
    if (IsDots(&info) || (keep_config && SameName(info.fname, kConfigFile))) {
      continue;
    }
    const uint16_t child = Append(path, length, info.fname);
    if (child == 0 || depth == kMaxDepth) {
      result = FR_INVALID_NAME;
      break;
    }
    if (info.fattrib & AM_DIR) {
      result = DeleteContents(path, child, (uint8_t)(depth + 1), false);
    }
    if (result == FR_OK) {
      result = f_unlink(path);
    }
    path[length] = '\0';
  }
  f_closedir(&dir);
  return result;
}

// The bytes of every file in the folder at path, at any depth.
static uint64_t SizeOf(
    char path[kFileListMaxPath + 1], uint16_t length, uint8_t depth) {
  uint64_t size = 0;
  DIR dir;
  if (f_opendir(&dir, path) != FR_OK) {
    return 0;
  }
  FILINFO info;
  while (f_readdir(&dir, &info) == FR_OK && IsEntry(&info)) {
    if (IsDots(&info)) {
      continue;
    }
    if (!(info.fattrib & AM_DIR)) {
      size += info.fsize;
      continue;
    }
    const uint16_t child = Append(path, length, info.fname);
    if (child != 0 && depth < kMaxDepth) {
      size += SizeOf(path, child, (uint8_t)(depth + 1));
    }
    path[length] = '\0';
  }
  f_closedir(&dir);
  return size;
}

// A copy under way: the buffer files go through, and how far it has come.
typedef struct Copy {
  uint8_t* buffer;
  uint32_t buffer_size;
  VMFolderProgress progress;
  uint64_t done;
  uint64_t total;
} Copy;

static FRESULT CopyFile(const char* from, const char* to, Copy* copy) {
  FIL in;
  FIL out;
  FRESULT result = f_open(&in, from, FA_READ);
  if (result != FR_OK) {
    return result;
  }
  result = f_open(&out, to, FA_WRITE | FA_CREATE_NEW);
  if (result != FR_OK) {
    f_close(&in);
    return result;
  }
  for (;;) {
    UINT read = 0;
    result = f_read(&in, copy->buffer, copy->buffer_size, &read);
    if (result != FR_OK || read == 0) {
      break;
    }
    UINT written = 0;
    result = f_write(&out, copy->buffer, read, &written);
    if (result == FR_OK && written < read) {
      // FatFs writes short when the card is full.
      result = FR_DENIED;
    }
    if (result != FR_OK) {
      break;
    }
    copy->done += read;
    copy->progress(
        (uint32_t)(copy->done / kBytesPerKB),
        (uint32_t)(copy->total / kBytesPerKB));
  }
  f_close(&in);
  const FRESULT closed = f_close(&out);
  return result != FR_OK ? result : closed;
}

// Copy what is in the folder at from into the one at to, which is there. Both
// paths are the caller's, added to and put back as for DeleteContents().
static FRESULT CopyContents(
    char from[kFileListMaxPath + 1], uint16_t from_length,
    char to[kFileListMaxPath + 1], uint16_t to_length, uint8_t depth,
    Copy* copy) {
  DIR dir;
  FRESULT result = f_opendir(&dir, from);
  FILINFO info;
  while (result == FR_OK && (result = f_readdir(&dir, &info)) == FR_OK &&
         IsEntry(&info)) {
    if (IsDots(&info)) {
      continue;
    }
    const uint16_t from_child = Append(from, from_length, info.fname);
    const uint16_t to_child = Append(to, to_length, info.fname);
    if (from_child == 0 || to_child == 0 || depth == kMaxDepth) {
      result = FR_INVALID_NAME;
    } else if (info.fattrib & AM_DIR) {
      result = f_mkdir(to);
      if (result == FR_OK) {
        result = CopyContents(
            from, from_child, to, to_child, (uint8_t)(depth + 1), copy);
      }
    } else {
      result = CopyFile(from, to, copy);
    }
    from[from_length] = '\0';
    to[to_length] = '\0';
  }
  f_closedir(&dir);
  return result;
}

VMFolderResult VMFolderClone(
    const VMEntry* vm, const char* name, uint8_t* buffer, uint32_t buffer_size,
    VMFolderProgress progress, char path[kFileListMaxPath + 1],
    char problem[kVMProblemLength + 1]) {
  FileListFolderOf(vm->path, g_from);
  if (!FolderFor(name, g_to, problem) || Taken(g_to, problem)) {
    return kVMFolderBadName;
  }
  FileListCopyPath(g_to, path);
  Append(path, Length(path), kConfigFile);
  const uint16_t from_length = Length(g_from);
  const uint16_t to_length = Length(g_to);
  Copy copy = {
      .buffer = buffer,
      .buffer_size = buffer_size,
      .progress = progress,
      .total = SizeOf(g_from, from_length, 0),
  };
  progress(0, (uint32_t)(copy.total / kBytesPerKB));
  FRESULT result = f_mkdir(g_to);
  if (result == FR_OK) {
    result = CopyContents(g_from, from_length, g_to, to_length, 0, &copy);
    if (result != FR_OK) {
      DeleteContents(g_to, to_length, 0, false);
      f_unlink(g_to);
    }
  }
  if (result != FR_OK) {
    Describe("copy", vm, result, problem);
    return kVMFolderFailed;
  }
  return Retarget(path, g_from, g_to, problem) ? kVMFolderDone
                                               : kVMFolderFailed;
}

bool VMFolderDelete(const VMEntry* vm, char problem[kVMProblemLength + 1]) {
  FileListFolderOf(vm->path, g_from);
  VMSettings settings;
  char ignored[kVMProblemLength + 1];
  VMSettingsRead(&settings, ignored);
  FRESULT result = DeleteContents(g_from, Length(g_from), 0, true);
  if (result == FR_OK) {
    result = f_unlink(vm->path);
  }
  if (result == FR_OK) {
    result = f_unlink(g_from);
  }
  if (result != FR_OK) {
    Describe("delete", vm, result, problem);
    return false;
  }
  if (InFolder(settings.boot_default, g_from)) {
    return VMSettingsSaveBootDefault("", problem);
  }
  return true;
}
