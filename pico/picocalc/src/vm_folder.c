#include "vm_folder.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ff.h"
#include "harddisk.h"

static const char kConfigFile[] = "vm.cfg";

// A blank disk's file, in its VM's folder, for each drive.
static const char* const kBlankFiles[kVMDrives] = {
    [kVMDriveA] = "fd1.img",
    [kVMDriveB] = "fd2.img",
    [kVMDriveHD1] = "hd1.img",
    [kVMDriveHD2] = "hd2.img",
};

enum {
  // A blank floppy's size: a 360KB disk.
  kFloppySize = 360 * 1024,
};

// Check a name typed for a VM, saying why it will not do, and make the VM's
// entry from it.
static bool TakeName(
    const char* name, VMEntry* entry, char problem[kVMProblemLength + 1]) {
  const size_t length = strlen(name);
  if (length == 0) {
    snprintf(problem, kVMProblemLength + 1, "Type a name for the VM");
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    if (length > kFSMaxFolderName || !FSIsFolderNameCharacter(name[i])) {
      snprintf(
          problem, kVMProblemLength + 1,
          "A name is up to %u letters, digits, - and _",
          (unsigned)kFSMaxFolderName);
      return false;
    }
  }
  if (FSIsDeviceName(name)) {
    snprintf(
        problem, kVMProblemLength + 1, "%s is the name of a DOS device", name);
    return false;
  }
  return VMEntryMake(name, entry);
}

// The folder a VM's file is in.
static void FolderOf(const VMEntry* vm, char folder[kFSMaxPath + 1]) {
  FSFolderOf(vm->path, folder);
}

static bool Taken(const VMEntry* vm, char problem[kVMProblemLength + 1]) {
  char folder[kFSMaxPath + 1];
  FolderOf(vm, folder);
  if (!FSExists(folder)) {
    return false;
  }
  snprintf(
      problem, kVMProblemLength + 1, "A VM named %s already exists", vm->name);
  return true;
}

// Why the card would not do something to a VM: "Could not copy DOS330: the
// card is full".
static void Describe(
    const char* verb, const VMEntry* vm, FRESULT result,
    char problem[kVMProblemLength + 1]) {
  snprintf(
      problem, kVMProblemLength + 1, "Could not %s %s: %s", verb, vm->name,
      FSDescribeResult(result));
}

// The VM read by Retarget() and VMFolderDescribe(): too large for the stack,
// with ConfigFileUpdate() under it.
static VMConfig g_config;

// Write again the drives of vm that name a file in from, to name it in to. A
// VM that does not read has what was read of it written again, and nothing
// else; one with nothing in from is not written.
static bool Retarget(
    const VMEntry* vm, const char* from, const char* to,
    char problem[kVMProblemLength + 1]) {
  char ignored[kVMProblemLength + 1];
  VMConfigRead(vm, &g_config, ignored);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    const char* path = g_config.drives[drive];
    if (!FSInFolder(path, from)) {
      continue;
    }
    char rebased[kFSMaxPath + 1];
    const int length =
        snprintf(rebased, sizeof(rebased), "%s%s", to, path + strlen(from));
    if (length < 0 || (size_t)length >= sizeof(rebased)) {
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

// A look through a VM's folder for VMFolderDescribe().
static bool CountFile(const FILINFO* info, void* context) {
  VMFolderContents* contents = context;
  if (strcasecmp(info->fname, kConfigFile) == 0) {
    return true;
  }
  if (contents->file_count < kVMFolderNamed) {
    snprintf(
        contents->files[contents->file_count],
        sizeof(contents->files[contents->file_count]), "%s", info->fname);
  }
  if (contents->file_count < UINT8_MAX) {
    ++contents->file_count;
  }
  return true;
}

void VMFolderDescribe(
    const VMList* list, const VMEntry* vm, VMFolderContents* contents) {
  contents->file_count = 0;
  contents->user_count = 0;
  char folder[kFSMaxPath + 1];
  FolderOf(vm, folder);
  FSListFolder(folder, CountFile, contents);
  for (uint8_t i = 0; i < list->count; ++i) {
    const VMEntry* other = &list->vms[i];
    if (strcasecmp(other->name, vm->name) == 0) {
      continue;
    }
    char ignored[kVMProblemLength + 1];
    VMConfigRead(other, &g_config, ignored);
    for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
      if (FSInFolder(g_config.drives[drive], folder)) {
        if (contents->user_count < kVMFolderNamed) {
          memcpy(
              contents->users[contents->user_count], other->name,
              sizeof(other->name));
        }
        ++contents->user_count;
        break;
      }
    }
  }
}

VMFolderResult VMFolderRename(
    const VMList* list, const VMEntry* vm, const char* name, VMEntry* renamed,
    char problem[kVMProblemLength + 1]) {
  if (!TakeName(name, renamed, problem)) {
    return kVMFolderRefused;
  }
  // The same name, perhaps in other capitals, which the card keeps as one.
  if (strcasecmp(vm->name, renamed->name) == 0) {
    *renamed = *vm;
    return kVMFolderDone;
  }
  if (Taken(renamed, problem)) {
    return kVMFolderRefused;
  }
  // Kept off the stack, which writing a configuration file under them takes
  // much of.
  static char from[kFSMaxPath + 1];
  static char to[kFSMaxPath + 1];
  static VMSettings settings;
  FolderOf(vm, from);
  FolderOf(renamed, to);
  char ignored[kVMProblemLength + 1];
  VMSettingsRead(&settings, ignored);
  const FRESULT result = f_rename(from, to);
  if (result != FR_OK) {
    Describe("rename", vm, result, problem);
    return kVMFolderFailed;
  }
  if (strcasecmp(settings.boot_default, vm->name) == 0 &&
      !VMSettingsSaveBootDefault(renamed->name, problem)) {
    return kVMFolderFailed;
  }
  for (uint8_t i = 0; i < list->count; ++i) {
    const VMEntry* other = &list->vms[i];
    if (!Retarget(
            strcasecmp(other->name, vm->name) == 0 ? renamed : other, from, to,
            problem)) {
      return kVMFolderFailed;
    }
  }
  return kVMFolderDone;
}

// The size of a blank disk for a drive.
static uint32_t BlankSize(uint8_t drive) {
  return drive <= kVMDriveB ? kFloppySize : Y_HDC_GEOMETRY_10MB_IMAGE_SIZE;
}

// The stem of a blank disk's file when its own name is taken, before _2 to
// _9: fd1_2.img, hd1_2.img.
static const char* const kBlankStems[kVMDrives] = {
    [kVMDriveA] = "fd1",
    [kVMDriveB] = "fd2",
    [kVMDriveHD1] = "hd1",
    [kVMDriveHD2] = "hd2",
};

// A free name in folder for a blank disk for a drive: its own, or else
// hd1_2.img to hd1_9.img and the like. Returns false if none is free.
static bool BlankPath(
    const char* folder, uint8_t drive, char path[kFSMaxPath + 1]) {
  if (FSJoin(folder, kBlankFiles[drive], path) && !FSExists(path)) {
    return true;
  }
  for (char n = '2'; n <= '9'; ++n) {
    char name[kFSMaxName + 1];
    snprintf(name, sizeof(name), "%s_%c.img", kBlankStems[drive], n);
    if (FSJoin(folder, name, path) && !FSExists(path)) {
      return true;
    }
  }
  return false;
}

// The blank disks' paths, and the drives' paths with them in: too large for
// the stack, with ConfigFileUpdate() under them.
static char g_blank_paths[kVMDrives][kFSMaxPath + 1];
static const char* g_paths[kVMDrives];

// Work out what vm's drives are to hold, in g_paths, refusing one image in
// two drives or a blank disk with no free name.
static bool PlanDrives(
    const VMEntry* vm, const char* folder, const VMFolderDrives* drives,
    FSTransfer* transfer, char problem[kVMProblemLength + 1]) {
  transfer->done = 0;
  transfer->total = 0;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_paths[drive] = drives->paths[drive] != NULL ? drives->paths[drive] : "";
    if (!drives->blank[drive]) {
      continue;
    }
    if (!BlankPath(folder, drive, g_blank_paths[drive])) {
      snprintf(
          problem, kVMProblemLength + 1, "No name is free in %s for %s",
          vm->name, kVMDriveLabels[drive]);
      return false;
    }
    g_paths[drive] = g_blank_paths[drive];
    transfer->total += BlankSize(drive);
  }
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    for (uint8_t earlier = 0; earlier < drive; ++earlier) {
      if (g_paths[drive][0] != '\0' &&
          FSSamePath(g_paths[drive], g_paths[earlier])) {
        char name[kFSMaxName + 1];
        const char* slash = strrchr(g_paths[drive], '/');
        snprintf(
            name, sizeof(name), "%s",
            slash != NULL ? slash + 1 : g_paths[drive]);
        snprintf(
            problem, kVMProblemLength + 1, "%s is in both %s and %s", name,
            kVMDriveLabels[earlier], kVMDriveLabels[drive]);
        return false;
      }
    }
  }
  return true;
}

// Write the blank disks planned, then vm's file naming every drive. Deletes
// the blank disks written if it fails.
static bool WriteDrives(
    const VMEntry* vm, const VMFolderDrives* drives, FSTransfer* transfer,
    char problem[kVMProblemLength + 1]) {
  if (transfer->total > 0) {
    transfer->progress(0, transfer->total);
  }
  FRESULT result = FR_OK;
  uint8_t written = 0;
  for (; written < kVMDrives && result == FR_OK; ++written) {
    if (drives->blank[written]) {
      result =
          FSWriteZeros(g_blank_paths[written], BlankSize(written), transfer);
    }
  }
  if (result != FR_OK) {
    Describe("make the disks of", vm, result, problem);
  } else if (VMConfigSaveDrives(vm, g_paths, problem)) {
    return true;
  }
  for (uint8_t drive = 0; drive < written; ++drive) {
    if (drives->blank[drive]) {
      f_unlink(g_blank_paths[drive]);
    }
  }
  return false;
}

VMFolderResult VMFolderCreate(
    const char* name, const VMFolderDrives* drives, FSTransfer* transfer,
    VMEntry* created, char problem[kVMProblemLength + 1]) {
  static char folder[kFSMaxPath + 1];
  if (!TakeName(name, created, problem) || Taken(created, problem)) {
    return kVMFolderRefused;
  }
  FolderOf(created, folder);
  if (!PlanDrives(created, folder, drives, transfer, problem)) {
    return kVMFolderRefused;
  }
  const FRESULT result = f_mkdir(folder);
  if (result != FR_OK) {
    Describe("make", created, result, problem);
    return kVMFolderFailed;
  }
  if (WriteDrives(created, drives, transfer, problem)) {
    return kVMFolderDone;
  }
  FSDeleteFolder(folder, NULL);
  return kVMFolderFailed;
}

VMFolderResult VMFolderEdit(
    const VMEntry* vm, const VMFolderDrives* drives, FSTransfer* transfer,
    char problem[kVMProblemLength + 1]) {
  static char folder[kFSMaxPath + 1];
  FolderOf(vm, folder);
  if (!PlanDrives(vm, folder, drives, transfer, problem)) {
    return kVMFolderRefused;
  }
  return WriteDrives(vm, drives, transfer, problem) ? kVMFolderDone
                                                    : kVMFolderFailed;
}

VMFolderResult VMFolderClone(
    const VMEntry* vm, const char* name, FSTransfer* transfer, VMEntry* copy,
    char problem[kVMProblemLength + 1]) {
  if (!TakeName(name, copy, problem) || Taken(copy, problem)) {
    return kVMFolderRefused;
  }
  static char from[kFSMaxPath + 1];
  static char to[kFSMaxPath + 1];
  FolderOf(vm, from);
  FolderOf(copy, to);
  transfer->done = 0;
  transfer->total = FSFolderSize(from);
  transfer->progress(0, transfer->total);
  const FRESULT result = FSCopyFolder(from, to, transfer);
  if (result != FR_OK) {
    Describe("copy", vm, result, problem);
    return kVMFolderFailed;
  }
  return Retarget(copy, from, to, problem) ? kVMFolderDone : kVMFolderFailed;
}

bool VMFolderDelete(const VMEntry* vm, char problem[kVMProblemLength + 1]) {
  static VMSettings settings;
  char ignored[kVMProblemLength + 1];
  VMSettingsRead(&settings, ignored);
  char folder[kFSMaxPath + 1];
  FolderOf(vm, folder);
  const FRESULT result = FSDeleteFolder(folder, kConfigFile);
  if (result != FR_OK) {
    Describe("delete", vm, result, problem);
    return false;
  }
  if (strcasecmp(settings.boot_default, vm->name) == 0) {
    return VMSettingsSaveBootDefault("", problem);
  }
  return true;
}
