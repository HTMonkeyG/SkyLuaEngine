#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "includes/htmodloader.h"

#include "aliases.h"
#include "skylua.h"

/**
 * Print the log to HTML console.
 */
static i32 logToHT(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  if (msg)
    HTTellText("[SkyLuaEngine][INFO] %s", msg);
  return 0;
}

i32 addLuaBindings(lua_State *L) {
  lua_getglobal(L, "sle");

  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_createtable(L, 0, 1);
    lua_setglobal(L, "sle");
    lua_getglobal(L, "sle");
  }

  lua_pushcfunction(L, logToHT);
  lua_setfield(L, -2, "log");

  lua_pop(L, 1);

  // Try to register our own print function.
  queueEval("_G.sle.print=function(...)_G.sle.log(table.concat({...},\"\t\"))end");

  return 1;
}
