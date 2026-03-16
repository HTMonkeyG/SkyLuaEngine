#include <stdlib.h>
#include <string.h>
#include <string>
#include <mutex>
#include <queue>

// Lua apis have no extern "C" declarations, so we add it manually.
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "skylua.h"

// Cached lua_State of the game.
lua_State *gGameLuaState = nullptr;

static std::queue<std::string> gScripts;
static std::mutex gScriptLock;

// Run script through the lua engine of the mod.
static bool runLuaLocalEngine(
  const char *script
) {
  if (!gGameLuaState || !script)
    return false;

  int top = lua_gettop(gGameLuaState);
  bool result = true;
  const char *error = nullptr;

  if (luaL_loadstring(gGameLuaState, script) != 0) {
    error = lua_tostring(gGameLuaState, -1);
    LOGW("Compilation err: %s\n", error);
    result = false;
  } else if (lua_pcall(gGameLuaState, 0, LUA_MULTRET, 0) != 0) {
    error = lua_tostring(gGameLuaState, -1);
    LOGW("Runtime err: %s\n", error);
    result = false;
  }

  lua_settop(gGameLuaState, top);

  return result;
}

// Run script through the lua engine of the game.
static bool runLuaSkyEngine(
  const char *script
) {
  return !lua_debugdostring(gGameLuaState, script);
}

i32 sleEvalAllQueued(
  i08 local
) {
  std::lock_guard<std::mutex> lock{gScriptLock};
  i32 result = 1;

  while (!gScripts.empty()) {
    bool r;
    std::string &script = gScripts.front();

    if (local)
      r = runLuaLocalEngine(script.c_str());
    else
      r = runLuaSkyEngine(script.c_str());

    gScripts.pop();

    if (!r)
      result = 0;
  }

  return result;
}

i32 sleQueueEval(
  const char *script
) {
  std::lock_guard<std::mutex> lock{gScriptLock};

  if (!script || !gGameLuaState)
    return 0;

  u64 len = strlen(script);
  if (!len)
    return 1;

  gScripts.push(std::string{script});

  return 1;
}
