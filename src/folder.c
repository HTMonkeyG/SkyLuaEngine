#include <windows.h>
#include <stdio.h>

#include "aliases.h"
#include "skylua.h"

static wchar_t gPathScripts[MAX_PATH]
  , gPathAutoExec[MAX_PATH];

static i32 folderExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

i32 initPaths() {
  wchar_t *p;

  if (!GetModuleFileNameW(hModuleDll, gPathScripts, MAX_PATH))
    return 0;

  p = wcsrchr(gPathScripts, L'\\');
  if (!p)
    return 0;
  *p = 0;

  wcscat_s(gPathScripts, MAX_PATH, L"\\scripts");

  wcscpy_s(gPathAutoExec, MAX_PATH, gPathScripts);
  wcscat_s(gPathAutoExec, MAX_PATH, L"\\autoexec");

  if (!folderExists(gPathScripts))
    CreateDirectoryW(gPathScripts, NULL);
  if (!folderExists(gPathAutoExec))
    CreateDirectoryW(gPathAutoExec, NULL);

  return 1;
}

i32 scanAutoExec() {
  HANDLE hFindFile;
  WIN32_FIND_DATAW findData;
  wchar_t path[MAX_PATH]
    , *p;
  char *content;
  u64 size;
  FILE *fd;

  // <gPathAutoExec>/*.lua
  wcscpy_s(path, MAX_PATH, gPathAutoExec);
  wcscat_s(path, MAX_PATH, L"\\*.lua");

  hFindFile = FindFirstFileW(path, &findData);
  if (!hFindFile)
    return 0;

  // <gPathAutoExec>/
  wcscpy_s(path, MAX_PATH, gPathAutoExec);
  wcscat_s(path, MAX_PATH, L"\\");
  p = &path[wcslen(path)];

  do {
    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;
    if (!wcscmp(findData.cFileName, L".") || !wcscmp(findData.cFileName, L".."))
      continue;

    // <gPathAutoExec>/<cFileName>
    wcscat_s(p, MAX_PATH, findData.cFileName);
    fd = _wfopen(path, L"rb");
    if (!fd)
      continue;

    // Read file.
    fseek(fd, 0, SEEK_END);
    size = ftell(fd);
    rewind(fd);
    content = (char *)malloc(size + 1);
    fread(content, sizeof(char), size, fd);
    content[size] = 0;

    fclose(fd);

    printf("Read script %ls\n", path);

    // Evaluate script after initialized.
    queueEval(content);

    free(content);
  } while (FindNextFileW(hFindFile, &findData));

  return 1;
}
