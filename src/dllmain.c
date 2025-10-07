#include <windows.h>
#include "MinHook.h"
#include "lua.h"
#include "lauxlib.h"
#include "includes/htmod.h"

#include "sigcodes.h"
#include "aliases.h"

CRITICAL_SECTION newScriptLock;
char *script = NULL;
volatile BOOL newScriptPresent = FALSE;

BOOL initialized = FALSE;
u64 *state;

typedef u64 (__fastcall *PFN_Lua_debugDoString)(
  u64 *, char *);
typedef u64 (__fastcall *PFN_Client_checkChangeLevel)(
  u64 **);

PFN_Lua_debugDoString fn_Lua_debugDoString;
PFN_Client_checkChangeLevel fn_Client_checkChangeLevel
  , tramp_Client_checkChangeLevel;

u64 __fastcall hook_Client_checkChangeLevel(
  u64 **a1
) {
  if (!initialized) {
    state = *(a1 + 4);
    initialized = TRUE;
  }

  EnterCriticalSection(&newScriptLock);
  if (newScriptPresent && state) {
    //fn_Lua_debugDoString(state, script);
    //printf("testaaa %d\n", luaL_dostring((lua_State *)state, script));
    //printf("testaaa %d\n", lua_resume((lua_State *)state, NULL, 1));
    if (luaL_loadstring((lua_State *)state, script) != 0) {
      const char* load_error = lua_tostring((lua_State *)state, -1);
      printf("编译错误: %s\n", load_error);
      lua_pop((lua_State *)state, 1);
    } else if (lua_pcall((lua_State *)state, 0, LUA_MULTRET, 0) != 0) {
      const char* runtime_error = lua_tostring((lua_State *)state, -1);
      printf("运行时错误: %s\n", runtime_error);
      lua_pop((lua_State *)state, 1);
    }
    newScriptPresent = FALSE;
  }
  LeaveCriticalSection(&newScriptLock);

  return tramp_Client_checkChangeLevel(a1);
}

BOOL queueEvaluate(char *input) {
  BOOL success = FALSE;

  EnterCriticalSection(&newScriptLock);
  if (!newScriptPresent) {
    if (script != NULL)
      free(script);

    script = (char *)malloc(strlen(input) + 1);
    if (script != NULL) {
      strcpy(script, input);
      newScriptPresent = TRUE;
      success = TRUE;
    }
  }
  LeaveCriticalSection(&newScriptLock);

  return success;
}

DWORD WINAPI ipcThread(void *) {
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
        queueEvaluate(buffer);
      }
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
  }

  return 0;
}

HTStatus HTMLAPI HTModOnInit(
  void *reserved
) {
  fn_Lua_debugDoString = HTSigScan(&sig_Lua_debugDoString);
  fn_Client_checkChangeLevel = HTSigScan(&sig_Client_checkChangeLevel);
  if (!fn_Client_checkChangeLevel || !fn_Lua_debugDoString)
    return HT_FAIL;

  MH_CreateHook(
    fn_Client_checkChangeLevel,
    (void *)hook_Client_checkChangeLevel,
    (void **)&tramp_Client_checkChangeLevel);
  MH_EnableHook(MH_ALL_HOOKS);
  CreateThread(NULL, 0, ipcThread, NULL, 0, NULL);

  return HT_SUCCESS;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    InitializeCriticalSection(&newScriptLock);
    MH_Initialize();
  } else if (dwReason == DLL_PROCESS_DETACH)
    DeleteCriticalSection(&newScriptLock);

  return TRUE;
}