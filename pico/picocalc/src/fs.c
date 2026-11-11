#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum {
  // How deep a folder copy or delete goes: folders in folders.
  kMaxDepth = 4,
};

// Names DOS gives its devices.
static const char* const kDeviceNames[] = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

void FSCopyPath(const char* from, char to[kFSMaxPath + 1]) {
  snprintf(to, kFSMaxPath + 1, "%s", from);
}

bool FSSamePath(const char* a, const char* b) { return strcasecmp(a, b) == 0; }

bool FSInFolder(const char* path, const char* folder) {
  const size_t length = strlen(folder);
  return strncasecmp(path, folder, length) == 0 && path[length] == '/';
}

void FSFolderOf(const char* path, char folder[kFSMaxPath + 1]) {
  FSCopyPath(path, folder);
  char* last_slash = strrchr(folder, '/');
  *(last_slash != NULL ? last_slash : folder) = '\0';
}

bool FSJoin(const char* folder, const char* name, char path[kFSMaxPath + 1]) {
  const int length =
      folder[0] == '\0' ? snprintf(path, kFSMaxPath + 1, "%s", name)
                        : snprintf(path, kFSMaxPath + 1, "%s/%s", folder, name);
  return length >= 0 && length <= kFSMaxPath;
}

bool FSResolvePath(
    const char* folder, const char* path, char resolved[kFSMaxPath + 1]) {
  uint16_t length = 0;
  if (path[0] == '/') {
    ++path;
  } else {
    for (const char* c = folder; *c != '\0'; ++c) {
      if (length == kFSMaxPath) {
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
      if (length == kFSMaxPath) {
        return false;
      }
      resolved[length++] = '/';
    }
    for (uint16_t i = 0; i < segment_length; ++i) {
      if (length == kFSMaxPath) {
        return false;
      }
      resolved[length++] = segment[i];
    }
  }
  resolved[length] = '\0';
  return true;
}

static bool PutChar(char out[kFSMaxPath + 2], uint16_t* length, char c) {
  if (*length == kFSMaxPath + 1) {
    return false;
  }
  out[(*length)++] = c;
  return true;
}

bool FSRelativePath(
    const char* folder, const char* path, char out[kFSMaxPath + 2]) {
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
    while (i < folder_length &&
           toupper((unsigned char)f[i]) == toupper((unsigned char)p[i])) {
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
    if (!PutChar(out, &length, (char)tolower((unsigned char)*p))) {
      return false;
    }
  }
  out[length] = '\0';
  return true;
}

bool FSExists(const char* path) {
  FILINFO info;
  return f_stat(path, &info) == FR_OK;
}

bool FSIsFolderNameCharacter(char c) {
  return isalnum((unsigned char)c) || c == '-' || c == '_';
}

bool FSIsDeviceName(const char* name) {
  for (size_t i = 0; i < sizeof(kDeviceNames) / sizeof(kDeviceNames[0]); ++i) {
    if (strcasecmp(name, kDeviceNames[i]) == 0) {
      return true;
    }
  }
  return false;
}

const char* FSDescribeResult(FRESULT result) {
  switch (result) {
    case FR_DENIED:
      return "the card is full";
    case FR_EXIST:
      return "it is already there";
    case FR_NO_FILE:
    case FR_NO_PATH:
      return "it is not there";
    case FR_WRITE_PROTECTED:
      return "the card is locked";
    case FR_INVALID_NAME:
      return "a path would be too long";
    default:
      return "the card failed";
  }
}

FRESULT FSListFolder(const char* path, FSVisit visit, void* context) {
  DIR dir;
  FRESULT result = f_opendir(&dir, path);
  if (result != FR_OK) {
    return result;
  }
  FILINFO info;
  while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
    if (strcmp(info.fname, ".") == 0 || strcmp(info.fname, "..") == 0) {
      continue;
    }
    if (!visit(&info, context)) {
      break;
    }
  }
  f_closedir(&dir);
  return result;
}

// ============================================================================
// Whole folders
// ============================================================================

// A walk over a folder, at any depth: one at a time. Its paths are added to
// as it goes down a folder and put back as it comes up, so a level costs the
// stack only its lengths - the paths are too long to keep a pair of each.
typedef struct Walk {
  char from[kFSMaxPath + 1];
  char to[kFSMaxPath + 1];
  FSTransfer* transfer;
  // A file directly in the folder deleted last, or NULL.
  const char* last;
  // The first failure, which stops the walk.
  FRESULT result;
  uint64_t size;
} Walk;

static Walk g_walk;

// A level of the walk: the folder's paths are g_walk's, this long.
typedef struct Level {
  uint16_t from_length;
  uint16_t to_length;
  uint8_t depth;
} Level;

// Add a name to the end of a path, after a slash, returning the new length,
// or 0 if it would not fit.
static uint16_t Append(char* path, uint16_t length, const char* name) {
  const int written =
      snprintf(&path[length], kFSMaxPath + 1 - length, "/%s", name);
  if (written < 0 || length + written > kFSMaxPath) {
    path[length] = '\0';
    return 0;
  }
  return (uint16_t)(length + written);
}

static bool AddSize(const FILINFO* info, void* context) {
  const Level* level = context;
  if (!(info->fattrib & AM_DIR)) {
    g_walk.size += info->fsize;
    return true;
  }
  Level below = {
      .from_length = Append(g_walk.from, level->from_length, info->fname),
      .depth = (uint8_t)(level->depth + 1),
  };
  if (below.from_length != 0 && below.depth <= kMaxDepth) {
    FSListFolder(g_walk.from, AddSize, &below);
  }
  g_walk.from[level->from_length] = '\0';
  return true;
}

uint64_t FSFolderSize(const char* path) {
  g_walk = (Walk){0};
  FSCopyPath(path, g_walk.from);
  Level top = {.from_length = (uint16_t)strlen(g_walk.from)};
  FSListFolder(g_walk.from, AddSize, &top);
  return g_walk.size;
}

static FRESULT CopyFile(
    const char* from, const char* to, FSTransfer* transfer) {
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
    result = f_read(&in, transfer->buffer, transfer->buffer_size, &read);
    if (result != FR_OK || read == 0) {
      break;
    }
    UINT written = 0;
    result = f_write(&out, transfer->buffer, read, &written);
    if (result == FR_OK && written < read) {
      // FatFs writes short when the card is full.
      result = FR_DENIED;
    }
    if (result != FR_OK) {
      break;
    }
    transfer->done += read;
    transfer->progress(transfer->done, transfer->total);
  }
  f_close(&in);
  const FRESULT closed = f_close(&out);
  return result != FR_OK ? result : closed;
}

static bool CopyEntry(const FILINFO* info, void* context) {
  const Level* level = context;
  Level below = {
      .from_length = Append(g_walk.from, level->from_length, info->fname),
      .to_length = Append(g_walk.to, level->to_length, info->fname),
      .depth = (uint8_t)(level->depth + 1),
  };
  if (below.from_length == 0 || below.to_length == 0 ||
      below.depth > kMaxDepth) {
    g_walk.result = FR_INVALID_NAME;
  } else if (info->fattrib & AM_DIR) {
    g_walk.result = f_mkdir(g_walk.to);
    if (g_walk.result == FR_OK) {
      const FRESULT listed = FSListFolder(g_walk.from, CopyEntry, &below);
      if (g_walk.result == FR_OK) {
        g_walk.result = listed;
      }
    }
  } else {
    g_walk.result = CopyFile(g_walk.from, g_walk.to, g_walk.transfer);
  }
  g_walk.from[level->from_length] = '\0';
  g_walk.to[level->to_length] = '\0';
  return g_walk.result == FR_OK;
}

static bool DeleteEntry(const FILINFO* info, void* context) {
  const Level* level = context;
  if (level->depth == 0 && g_walk.last != NULL &&
      strcasecmp(info->fname, g_walk.last) == 0) {
    return true;
  }
  Level below = {
      .from_length = Append(g_walk.from, level->from_length, info->fname),
      .depth = (uint8_t)(level->depth + 1),
  };
  if (below.from_length == 0 || below.depth > kMaxDepth) {
    g_walk.result = FR_INVALID_NAME;
  } else {
    if (info->fattrib & AM_DIR) {
      const FRESULT listed = FSListFolder(g_walk.from, DeleteEntry, &below);
      if (g_walk.result == FR_OK) {
        g_walk.result = listed;
      }
    }
    if (g_walk.result == FR_OK) {
      g_walk.result = f_unlink(g_walk.from);
    }
  }
  g_walk.from[level->from_length] = '\0';
  return g_walk.result == FR_OK;
}

FRESULT FSDeleteFolder(const char* path, const char* last) {
  g_walk = (Walk){.last = last};
  FSCopyPath(path, g_walk.from);
  Level top = {.from_length = (uint16_t)strlen(g_walk.from)};
  FRESULT result = FSListFolder(g_walk.from, DeleteEntry, &top);
  if (g_walk.result != FR_OK) {
    return g_walk.result;
  }
  if (result == FR_OK && last != NULL &&
      Append(g_walk.from, top.from_length, last) != 0 &&
      FSExists(g_walk.from)) {
    result = f_unlink(g_walk.from);
  }
  g_walk.from[top.from_length] = '\0';
  if (result == FR_OK) {
    result = f_unlink(g_walk.from);
  }
  return result;
}

FRESULT FSCopyFolder(const char* from, const char* to, FSTransfer* transfer) {
  g_walk = (Walk){.transfer = transfer};
  FSCopyPath(from, g_walk.from);
  FSCopyPath(to, g_walk.to);
  FRESULT result = f_mkdir(g_walk.to);
  if (result != FR_OK) {
    return result;
  }
  Level top = {
      .from_length = (uint16_t)strlen(g_walk.from),
      .to_length = (uint16_t)strlen(g_walk.to),
  };
  result = FSListFolder(g_walk.from, CopyEntry, &top);
  if (g_walk.result != FR_OK) {
    result = g_walk.result;
  }
  if (result != FR_OK) {
    FSDeleteFolder(to, NULL);
  }
  return result;
}
