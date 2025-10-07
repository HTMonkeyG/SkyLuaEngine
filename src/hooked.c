#include <windows.h>
#include "MinHook.h"
#include "skylua.h"
#include "sigcodes.h"

PFN_Lua_debugDoString fn_Lua_debugDoString;

static BOOL initialized = FALSE;
static PFN_Client_checkChangeLevel fn_Client_checkChangeLevel
  , tramp_Client_checkChangeLevel;

static u64 __fastcall hook_Client_checkChangeLevel(
  u64 **a1
) {
  if (!initialized) {
    gGameLuaState = *(lua_State **)(a1 + 4);
    initialized = TRUE;
  }

  evalAllQueued(gUseLocalEngine);

  return tramp_Client_checkChangeLevel(a1);
}

i32 initAllHooks() {
  fn_Lua_debugDoString = HTSigScan(&sig_Lua_debugDoString);
  fn_Client_checkChangeLevel = HTSigScan(&sig_Client_checkChangeLevel);
  if (!fn_Client_checkChangeLevel || !fn_Lua_debugDoString)
    return 0;

  MH_CreateHook(
    fn_Client_checkChangeLevel,
    (void *)hook_Client_checkChangeLevel,
    (void **)&tramp_Client_checkChangeLevel);
  MH_EnableHook(MH_ALL_HOOKS);

  return 1;
}
