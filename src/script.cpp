#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <queue>

#include "includes/htmodloader.h"

// Lua apis have no extern "C" declarations, so we add it manually.
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "aliases.h"
#include "skylua.h"

lua_State *gGameLuaState;

static std::queue<char *> gScripts;
static lua_State *gLocalLuaState;
static std::mutex gScriptLock;

static bool runLuaLocalEngine(
  const char *script
) {
  if (!gGameLuaState || !script)
    return false;

  int top = lua_gettop(gGameLuaState);
  bool result = true;

  if (luaL_loadstring(gGameLuaState, script) != 0) {
    const char* load_error = lua_tostring(gGameLuaState, -1);
    HTTellText("[SkyLuaEngine][WARN] Compilation err: %s\n", load_error);
    result = false;
  } else if (lua_pcall(gGameLuaState, 0, LUA_MULTRET, 0) != 0) {
    const char* runtime_error = lua_tostring(gGameLuaState, -1);
    HTTellText("[SkyLuaEngine][WARN] Runtime err: %s\n", runtime_error);
    result = false;
  }

  lua_settop(gGameLuaState, top);

  return result;
}

static bool runLuaSkyEngine(
  const char *script
) {
  if (!fn_Lua_debugDoString || !gGameLuaState || !script)
    return false;
  return !(i32)fn_Lua_debugDoString(gGameLuaState, script);
}

i32 evalAllQueued(i08 local) {
  std::lock_guard<std::mutex> lock(gScriptLock);
  i32 result = 1;

  while (!gScripts.empty()) {
    bool r;
    char *script = gScripts.front();

    if (local)
      r = runLuaLocalEngine(script);
    else
      r = runLuaSkyEngine(script);

    free(script);
    gScripts.pop();

    if (!r)
      result = 0;
  }

  return result;
}

i32 queueEval(
  const char *script
) {
  std::lock_guard<std::mutex> lock(gScriptLock);

  if (!script)
    return 0;
  
  u64 len = strlen(script);
  if (!len)
    return 1;

  char *tmp = (char *)malloc(len + 1);

  if (!tmp)
    return 0;
  
  strcpy(tmp, script);
  gScripts.push(tmp);

  return 1;
}

static bool runScriptLocal(
  const char *script
) {
  return false;
}

i32 evalLocal() {
  return 0;
}
