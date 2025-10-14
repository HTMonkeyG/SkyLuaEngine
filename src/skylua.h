#ifndef __SKYLUA_H__
#define __SKYLUA_H__

#include <windows.h>
#include "lua.h"
#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u64 (__fastcall *PFN_Lua_debugDoString)(
  lua_State *, const char *);
typedef u64 (__fastcall *PFN_Client_checkChangeLevel)(
  u64 **);

extern HMODULE hModuleDll;
extern lua_State *gGameLuaState;
extern PFN_Lua_debugDoString fn_Lua_debugDoString;
extern i08 gUseLocalEngine;

// Install hooks.
i32 initAllHooks();
// Initialize GUI and inputs.
i32 initGui();
// Get or create script folders.
i32 initPaths();

// Push all scripts in <ModFolder>/scripts/autoexec into the queue.
i32 scanAutoExec();

// Evaluate all queued scripts in-game. Use local lua engine to interpret the
// scripts when local is true.
// Returns 0 when any of the scripts failed.
// This function should ONLY be called in the main thread of the game to avoid
// race conditions.
i32 evalAllQueued(
  i08 local);

// Push a script onto the stack to be executed.
i32 queueEval(
  const char *script);

// Add lua bindings.
i32 addLuaBindings(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
