#ifndef __SKYLUA_H__
#define __SKYLUA_H__

#include <windows.h>
#include "includes/htmodloader.h"
#include "lua.h"
#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGI(s, ...) HTTellText("[SkyLuaEngine][INFO] " s, ## __VA_ARGS__)
#define LOGW(s, ...) HTTellText("[SkyLuaEngine][WARN] " s, ## __VA_ARGS__)

extern HMODULE hModuleDll;
extern lua_State *gGameLuaState;
extern i08 gUseLocalEngine;

// Install hooks.
i32 sleInitAllHooks();

// Initialize GUI and inputs.
i32 sleInitGui();

// Get or create script folders.
i32 sleInitPaths();

// Add lua C binding functions.
i32 sleAddLuaBindings(
  lua_State *L);


// Push all scripts in <ModFolder>/scripts/autoexec into the queue.
i32 sleScanAutoExec();

// Evaluate all queued scripts in-game. Use local lua engine to interpret the
// scripts when local is true.
// Returns 0 when any of the scripts failed.
// This function should ONLY be called in the main thread of the game to avoid
// race conditions.
i32 sleEvalAllQueued(
  i08 local);

// Push a script onto the stack to be executed.
i32 sleQueueEval(
  const char *script);

// Call lua_debugdostring() of the game. Returns 1 if failed.
u32 lua_debugdostring(
  lua_State *L,
  const char *s);


// Draw ImGui menus.
void sleRenderGui();

#ifdef __cplusplus
}
#endif

#endif
