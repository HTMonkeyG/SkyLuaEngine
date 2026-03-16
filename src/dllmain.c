#include <windows.h>
#include "includes/htmod.h"

#include "skylua.h"

HMODULE hModuleDll;

static DWORD WINAPI ipcThread(void *) {
  char pipeName[] = "\\\\.\\pipe\\sky_research";
  char buffer[131072];
  DWORD bytesRead;

  while (1) {
    HANDLE hPipe = CreateNamedPipeA(
      pipeName,
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES,
      131072,
      131072,
      0,
      NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
      continue;

    if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
      while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        sleQueueEval(buffer);
      }
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
  }

  return 0;
}

__declspec(dllexport) HTStatus HTMLAPI HTModOnInit(
  void *reserved
) {
  if (!sleInitAllHooks())
    return HT_FAIL;

  sleInitGui();
  sleInitPaths();
  sleScanAutoExec();

  CreateThread(NULL, 0, ipcThread, NULL, 0, NULL);

  return HT_SUCCESS;
}

__declspec(dllexport) void HTMLAPI HTModRenderGui(
  float timeElapesed,
  void *reserved
) {
  sleRenderGui();
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    hModuleDll = hModule;
    DisableThreadLibraryCalls(hModule);
  }
  return TRUE;
}
