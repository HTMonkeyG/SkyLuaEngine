#include "skylua.h"

typedef u64 (__fastcall *PFN_lua_debugdostring)(
  lua_State *, const char *);
typedef void (__fastcall *PFN_Game_Update)(
  u64 **);

PFN_lua_debugdostring fn_lua_debugdostring;

static const HTAsmSig sigE8_lua_debugdostring = {
  .sig = 
    "75 CC 48 8D 15 ?  ?  ?  ?  48 89 F1 E8 ",
  .indirect = HTSigScanType_E8,
  .offset = 0x0C
};

static const HTAsmSig sigE8_Game_UpdateSync = {
  .sig = 
    "48 89 F1 E8 ?  ?  ?  ?  48 89 F1 E8 ?  ?  ?  ?  "
    "E8 ?  ?  ?  ?  E8 ",
  .indirect = HTSigScanType_E8,
  .offset = 0x03
};

static BOOL initialized = FALSE;
static HTAsmFunction sfn_Game_UpdateSync = {
  .name = "Game::UpdateSync",
  .fn = NULL,
  .origin = NULL,
  .detour = NULL
};

static void __fastcall hook_Game_UpdateSync(
  u64 **a1
) {
  if (!initialized && a1) {
    gGameLuaState = *(lua_State **)(a1 + 4);
    sleAddLuaBindings(gGameLuaState);
    initialized = TRUE;
    LOGI("gGameLuaState = %p", gGameLuaState);
  }

  sleEvalAllQueued(gUseLocalEngine);

  return ((PFN_Game_Update)sfn_Game_UpdateSync.origin)(a1);
}

u32 lua_debugdostring(
  lua_State *L,
  const char *s
) {
  return fn_lua_debugdostring(L, s);
}

i32 sleInitAllHooks() {
  fn_lua_debugdostring = HTSigScan(&sigE8_lua_debugdostring);
  if (!fn_lua_debugdostring)
    return 0;
  
  LOGI("Scanned lua_debugdostring(): %p", fn_lua_debugdostring);

  HTSigScanFunc(
    &sigE8_Game_UpdateSync,
    &sfn_Game_UpdateSync);
  
  sfn_Game_UpdateSync.detour = (void *)hook_Game_UpdateSync;
  if (!HTAsmHookCreate(hModuleDll, &sfn_Game_UpdateSync))
    return 0;
  if (!HTAsmHookEnable(hModuleDll, sfn_Game_UpdateSync.fn))
    return 0;

  LOGI("Created hook at Game::UpdateSync(): %p", sfn_Game_UpdateSync.fn);

  return 1;
}
