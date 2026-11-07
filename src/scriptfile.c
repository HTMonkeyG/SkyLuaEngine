#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "skylua.h"

// Script file access for the built-in editor. All names passed to this module
// are UTF-8 paths relative to <ModFolder>/scripts.

#define SLE_MAX_SCAN_DEPTH 8

typedef struct SleNameList {
  char **names;
  u32 count;
  u32 cap;
} SleNameList;

static char *wideToUtf8(const wchar_t *text) {
  int len;
  char *out;

  if (!text)
    return NULL;

  len = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
  if (len <= 0)
    return NULL;

  out = (char *)malloc((size_t)len);
  if (!out)
    return NULL;

  if (WideCharToMultiByte(
      CP_UTF8, 0, text, -1, out, len, NULL, NULL) <= 0) {
    free(out);
    return NULL;
  }

  return out;
}

static wchar_t *utf8ToWide(const char *text) {
  int len;
  wchar_t *out;

  if (!text)
    return NULL;

  len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (len <= 0)
    return NULL;

  out = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)len);
  if (!out)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, out, len) <= 0) {
    free(out);
    return NULL;
  }

  return out;
}

// Reject absolute paths and parent directory traversal.
static i32 isSafeRelativeName(const char *name) {
  const char *p;

  if (!name || !name[0])
    return 0;
  if (name[0] == '/' || name[0] == '\\')
    return 0;
  if (name[1] == ':')
    return 0;

  p = name;
  while (*p) {
    const char *start = p;
    size_t len;

    while (*p && *p != '/' && *p != '\\') {
      if (*p == ':')
        return 0;
      ++p;
    }

    len = (size_t)(p - start);
    if (len == 0)
      return 0;
    if (len == 1 && start[0] == '.')
      return 0;
    if (len == 2 && start[0] == '.' && start[1] == '.')
      return 0;

    if (*p == '/' || *p == '\\')
      ++p;
  }

  return 1;
}

// Build a wide absolute path from a UTF-8 relative script name.
static i32 buildScriptPath(
  const char *relName,
  wchar_t *out,
  u32 outCount
) {
  const wchar_t *root;
  wchar_t *wideName;
  wchar_t *p;
  int result;

  if (!isSafeRelativeName(relName) || !out || outCount == 0)
    return 0;

  root = sleGetScriptsPath();
  if (!root || !root[0])
    return 0;

  wideName = utf8ToWide(relName);
  if (!wideName)
    return 0;

  for (p = wideName; *p; ++p) {
    if (*p == L'/')
      *p = L'\\';
  }

  result = _snwprintf(out, outCount, L"%ls\\%ls", root, wideName);
  free(wideName);

  if (result < 0 || (u32)result >= outCount)
    return 0;

  return 1;
}

// Create the directories between the scripts root and the target file.
static void createParentDirectories(const wchar_t *filePath) {
  const wchar_t *root;
  wchar_t buffer[MAX_PATH];
  wchar_t *slash;
  size_t rootLen;
  size_t i;

  if (!filePath || wcscpy_s(buffer, MAX_PATH, filePath) != 0)
    return;

  slash = wcsrchr(buffer, L'\\');
  if (!slash)
    return;
  *slash = 0;

  root = sleGetScriptsPath();
  if (!root)
    return;

  rootLen = wcslen(root);
  if (wcslen(buffer) <= rootLen)
    return;

  for (i = rootLen + 1; buffer[i]; ++i) {
    if (buffer[i] == L'\\') {
      wchar_t saved = buffer[i];

      buffer[i] = 0;
      CreateDirectoryW(buffer, NULL);
      buffer[i] = saved;
    }
  }

  CreateDirectoryW(buffer, NULL);
}

i32 sleReadScriptFile(
  const char *relName,
  char **outData,
  u64 *outSize
) {
  wchar_t path[MAX_PATH];
  FILE *fd;
  long size;
  char *data;
  size_t read;

  if (outData)
    *outData = NULL;
  if (outSize)
    *outSize = 0;
  if (!outData || !buildScriptPath(relName, path, MAX_PATH))
    return 0;

  fd = _wfopen(path, L"rb");
  if (!fd)
    return 0;

  if (fseek(fd, 0, SEEK_END) != 0) {
    fclose(fd);
    return 0;
  }

  size = ftell(fd);
  if (size < 0) {
    fclose(fd);
    return 0;
  }

  rewind(fd);
  data = (char *)malloc((size_t)size + 1);
  if (!data) {
    fclose(fd);
    return 0;
  }

  read = fread(data, 1, (size_t)size, fd);
  fclose(fd);

  data[read] = 0;
  *outData = data;
  if (outSize)
    *outSize = (u64)read;

  return 1;
}

i32 sleWriteScriptFile(
  const char *relName,
  const char *data,
  u64 size
) {
  wchar_t path[MAX_PATH];
  FILE *fd;

  if (!data || !buildScriptPath(relName, path, MAX_PATH))
    return 0;

  createParentDirectories(path);

  fd = _wfopen(path, L"wb");
  if (!fd)
    return 0;

  if (size && fwrite(data, 1, (size_t)size, fd) != (size_t)size) {
    fclose(fd);
    return 0;
  }

  fclose(fd);

  return 1;
}

static i32 nameListAdd(SleNameList *list, const wchar_t *relName) {
  char *utf8;

  if (list->count == list->cap) {
    u32 newCap = list->cap ? list->cap * 2 : 16;
    char **newNames = (char **)realloc(
      list->names, sizeof(char *) * (size_t)newCap);

    if (!newNames)
      return 0;

    list->names = newNames;
    list->cap = newCap;
  }

  utf8 = wideToUtf8(relName);
  if (!utf8)
    return 0;

  // Use forward slashes consistently so the same file always maps to the
  // same tab regardless of how its name was entered.
  {
    char *p;
    for (p = utf8; *p; ++p) {
      if (*p == '\\')
        *p = '/';
    }
  }

  list->names[list->count++] = utf8;

  return 1;
}

static void scanDirectory(
  const wchar_t *absDir,
  const wchar_t *relPrefix,
  SleNameList *list,
  u32 depth
) {
  wchar_t pattern[MAX_PATH];
  wchar_t absChild[MAX_PATH];
  wchar_t relChild[MAX_PATH];
  WIN32_FIND_DATAW findData;
  HANDLE hFind;

  if (depth > SLE_MAX_SCAN_DEPTH)
    return;

  _snwprintf(pattern, MAX_PATH, L"%ls\\*", absDir);
  pattern[MAX_PATH - 1] = 0;

  hFind = FindFirstFileW(pattern, &findData);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do {
    if (!wcscmp(findData.cFileName, L".")
        || !wcscmp(findData.cFileName, L".."))
      continue;

    // Skip hidden files and directories.
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
      continue;

    if (relPrefix && relPrefix[0])
      _snwprintf(
        relChild, MAX_PATH, L"%ls\\%ls", relPrefix, findData.cFileName);
    else
      _snwprintf(relChild, MAX_PATH, L"%ls", findData.cFileName);
    relChild[MAX_PATH - 1] = 0;

    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      _snwprintf(
        absChild, MAX_PATH, L"%ls\\%ls", absDir, findData.cFileName);
      absChild[MAX_PATH - 1] = 0;

      scanDirectory(absChild, relChild, list, depth + 1);
      continue;
    }

    {
      const wchar_t *ext = wcsrchr(findData.cFileName, L'.');
      if (!ext || _wcsicmp(ext, L".lua"))
        continue;
    }

    nameListAdd(list, relChild);
  } while (FindNextFileW(hFind, &findData));

  FindClose(hFind);
}

static int compareNames(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}

u32 sleListScriptFiles(char ***outNames) {
  SleNameList list;
  const wchar_t *root;
  u32 i;

  if (outNames)
    *outNames = NULL;

  root = sleGetScriptsPath();
  if (!root || !root[0])
    return 0;

  list.names = NULL;
  list.count = 0;
  list.cap = 0;

  scanDirectory(root, L"", &list, 0);

  if (list.count > 1)
    qsort(list.names, list.count, sizeof(char *), compareNames);

  if (outNames) {
    *outNames = list.names;
  } else {
    for (i = 0; i < list.count; ++i)
      free(list.names[i]);
    free(list.names);
  }

  return list.count;
}

void sleFreeScriptFiles(char **names, u32 count) {
  u32 i;

  if (!names)
    return;

  for (i = 0; i < count; ++i)
    free(names[i]);

  free(names);
}
