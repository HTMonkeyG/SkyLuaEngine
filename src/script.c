#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "skylua.h"

// Cached lua_State of the game.
lua_State *gGameLuaState = NULL;

typedef struct SleQueueItem {
  struct SleQueueItem *next;
  char *code;
  i08 repl;
} SleQueueItem;

static SleQueueItem *gQueueHead;
static SleQueueItem *gQueueTail;
static CRITICAL_SECTION gQueueLock;
static i08 gQueueLockReady = 0;

void sleInitQueue(void) {
  if (gQueueLockReady)
    return;

  InitializeCriticalSection(&gQueueLock);
  gQueueLockReady = 1;
}

static i32 queuePush(const char *script, i08 repl) {
  SleQueueItem *item;
  size_t len;

  if (!script)
    return 0;

  len = strlen(script);
  if (!len)
    return 1;

  if (!gQueueLockReady)
    sleInitQueue();

  item = (SleQueueItem *)malloc(sizeof(SleQueueItem));
  if (!item)
    return 0;

  item->code = (char *)malloc(len + 1);
  if (!item->code) {
    free(item);
    return 0;
  }

  memcpy(item->code, script, len + 1);
  item->repl = repl;
  item->next = NULL;

  EnterCriticalSection(&gQueueLock);
  if (gQueueTail)
    gQueueTail->next = item;
  else
    gQueueHead = item;
  gQueueTail = item;
  LeaveCriticalSection(&gQueueLock);

  return 1;
}

// Run a script through the lua engine embedded in the mod.
static i08 runLuaLocalEngine(const char *script) {
  int top;
  i08 result = 1;

  if (!gGameLuaState || !script)
    return 0;

  top = lua_gettop(gGameLuaState);

  if (luaL_loadstring(gGameLuaState, script) != 0) {
    const char *error = lua_tostring(gGameLuaState, -1);
    LOGW("Compilation err: %s\n", error ? error : "(unknown error)");
    result = 0;
  } else if (lua_pcall(gGameLuaState, 0, LUA_MULTRET, 0) != 0) {
    const char *error = lua_tostring(gGameLuaState, -1);
    LOGW("Runtime err: %s\n", error ? error : "(unknown error)");
    result = 0;
  }

  lua_settop(gGameLuaState, top);

  return result;
}

// Run a script through the lua engine of the game.
static i08 runLuaSkyEngine(const char *script) {
  return !lua_debugdostring(gGameLuaState, script);
}

i32 sleEvalAllQueued(i08 local) {
  SleQueueItem *item;
  SleQueueItem *next;
  i32 result = 1;

  if (!gQueueLockReady)
    sleInitQueue();

  // Detach the whole list so producers are not blocked while scripts run.
  EnterCriticalSection(&gQueueLock);
  item = gQueueHead;
  gQueueHead = NULL;
  gQueueTail = NULL;
  LeaveCriticalSection(&gQueueLock);

  while (item) {
    i08 ok;

    next = item->next;

    if (item->repl)
      // REPL snippets always run through the local engine so their output can
      // be captured instead of being written to the game log.
      ok = sleReplExec(gGameLuaState, item->code);
    else if (local)
      ok = runLuaLocalEngine(item->code);
    else
      ok = runLuaSkyEngine(item->code);

    if (!ok)
      result = 0;

    free(item->code);
    free(item);
    item = next;
  }

  return result;
}

i32 sleQueueEval(const char *script) {
  // Scripts queued before the game's Lua state is available (for example
  // autoexec scripts scanned during mod initialization) are executed by the
  // first UpdateSync call after the state is initialized.
  if (!script)
    return 0;

  return queuePush(script, 0);
}

i32 sleQueueRepl(const char *script) {
  if (!script || !gGameLuaState)
    return 0;

  return queuePush(script, 1);
}
