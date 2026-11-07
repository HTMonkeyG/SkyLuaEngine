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

// Get the absolute path of <ModFolder>/scripts.
const wchar_t *sleGetScriptsPath();
const char *sleGetScriptsPathUtf8();

// Add lua C binding functions. This also registers _G.sle = {} in the lua
// state of the game.
i32 sleAddLuaBindings(
  lua_State *L);


// Push all scripts in <ModFolder>/scripts/autoexec into the queue.
i32 sleScanAutoExec();

// Initialize the script queue. Safe to call more than once.
void sleInitQueue(void);

// Evaluate all queued scripts in-game. Use local lua engine to interpret the
// scripts when local is true.
// Returns 0 when any of the scripts failed.
// This function should ONLY be called in the main thread of the game to avoid
// race conditions.
i32 sleEvalAllQueued(
  i08 local);

// Push a script onto the stack to be executed. Scripts queued before the
// game's Lua state is available are executed once it has been initialized.
i32 sleQueueEval(
  const char *script);

// Push a REPL snippet onto the queue. The output of the snippet is captured by
// the REPL terminal instead of being sent to the HTModLoader console.
i32 sleQueueRepl(
  const char *script);

// Call lua_debugdostring() of the game. Returns 1 if failed.
u32 lua_debugdostring(
  lua_State *L,
  const char *s);


// Execute one REPL snippet on the calling thread.
i32 sleReplExec(
  lua_State *L,
  const char *code);

// REPL terminal buffer.
void sleReplAppend(
  const char *text,
  u64 len);
void sleReplLog(
  const char *fmt,
  ...);
void sleReplClear(void);
const char *sleReplGetText(void);
u64 sleReplGetVersion(void);
i08 sleReplIsCapturing(void);
void sleReplSetCapturing(
  i08 enabled);

// Send a message to the REPL terminal when a REPL snippet is running,
// otherwise send it to the HTModLoader console.
void sleLogText(
  const char *text);


// Script files under <ModFolder>/scripts. Names are UTF-8 paths relative to
// the scripts folder.
u32 sleListScriptFiles(
  char ***outNames);
void sleFreeScriptFiles(
  char **names,
  u32 count);
i32 sleReadScriptFile(
  const char *relName,
  char **outData,
  u64 *outSize);
i32 sleWriteScriptFile(
  const char *relName,
  const char *data,
  u64 size);


// Draw ImGui menus.
void sleRenderGui();

#ifdef __cplusplus
}
#endif

#endif
