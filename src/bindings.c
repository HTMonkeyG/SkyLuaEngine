#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "skylua.h"

// Convert all arguments to strings and join them with tabs, then send the
// result to the REPL terminal (during a REPL snippet) or the HT console.
static i32 sleLuaPrint(lua_State *L) {
  int count = lua_gettop(L);
  int i;
  size_t total = 1;
  char *buffer;
  char *cursor;

  // Measure the converted arguments first.
  for (i = 1; i <= count; ++i) {
    size_t len = 0;
    luaL_tolstring(L, i, &len);
    total += len + 1;
    lua_pop(L, 1);
  }

  buffer = (char *)malloc(total);
  if (!buffer)
    return 0;

  cursor = buffer;

  for (i = 1; i <= count; ++i) {
    size_t len = 0;
    const char *text = luaL_tolstring(L, i, &len);

    if (i > 1)
      *cursor++ = '\t';
    if (text && len) {
      memcpy(cursor, text, len);
      cursor += len;
    }
    lua_pop(L, 1);
  }

  *cursor = 0;
  sleLogText(buffer);
  free(buffer);

  return 0;
}

i32 sleAddLuaBindings(lua_State *L) {
  // Register the namespace: _G.sle = {}
  lua_newtable(L);
  lua_setglobal(L, "sle");

  lua_getglobal(L, "sle");

  lua_pushcfunction(L, sleLuaPrint);
  lua_setfield(L, -2, "log");

  // sle.print() mirrors print() but always respects the REPL capture state.
  lua_pushcfunction(L, sleLuaPrint);
  lua_setfield(L, -2, "print");

  lua_pop(L, 1);

  return 1;
}
