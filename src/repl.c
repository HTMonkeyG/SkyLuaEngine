#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lua.h"
#include "lauxlib.h"

#include "skylua.h"

// REPL terminal buffer. Logs produced while a REPL snippet is running are
// captured here instead of being forwarded to the HTModLoader console.
#define SLE_REPL_LOG_MAX (64u * 1024u)

static char *gReplText;
static u64 gReplTextLen;
static u64 gReplTextCap;
static u64 gReplVersion = 1;
static i08 gReplCapturing = 0;

static i32 replReserve(u64 extra) {
  u64 needed;
  u64 cap;
  char *buffer;

  needed = gReplTextLen + extra + 1;
  if (needed <= gReplTextCap)
    return 1;

  cap = gReplTextCap ? gReplTextCap : 4096;
  while (cap < needed)
    cap *= 2;

  buffer = (char *)realloc(gReplText, (size_t)cap);
  if (!buffer)
    return 0;

  gReplText = buffer;
  gReplTextCap = cap;

  return 1;
}

void sleReplAppend(const char *text, u64 len) {
  if (!text || !len || !replReserve(len))
    return;

  memcpy(gReplText + gReplTextLen, text, (size_t)len);
  gReplTextLen += len;
  gReplText[gReplTextLen] = 0;
  ++gReplVersion;

  // Drop the oldest output, keeping the buffer at a line boundary.
  if (gReplTextLen > SLE_REPL_LOG_MAX) {
    u64 drop = gReplTextLen - SLE_REPL_LOG_MAX;
    char *newline = (char *)memchr(
      gReplText + drop, '\n', (size_t)(gReplTextLen - drop));

    if (newline)
      drop = (u64)(newline - gReplText) + 1;

    memmove(
      gReplText, gReplText + drop, (size_t)(gReplTextLen - drop + 1));
    gReplTextLen -= drop;
  }
}

void sleReplLog(const char *fmt, ...) {
  char stackBuffer[1024];
  char *buffer;
  va_list args;
  int needed;

  if (!fmt)
    return;

  va_start(args, fmt);
  needed = vsnprintf(stackBuffer, sizeof(stackBuffer), fmt, args);
  va_end(args);

  if (needed < 0)
    return;

  if ((size_t)needed < sizeof(stackBuffer)) {
    sleReplAppend(stackBuffer, (u64)needed);
  } else {
    buffer = (char *)malloc((size_t)needed + 1);
    if (!buffer)
      return;

    va_start(args, fmt);
    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    va_end(args);

    sleReplAppend(buffer, (u64)needed);
    free(buffer);
  }

  sleReplAppend("\n", 1);
}

void sleReplClear(void) {
  gReplTextLen = 0;
  if (gReplText)
    gReplText[0] = 0;
  ++gReplVersion;
}

const char *sleReplGetText(void) {
  return gReplText ? gReplText : "";
}

u64 sleReplGetVersion(void) {
  return gReplVersion;
}

i08 sleReplIsCapturing(void) {
  return gReplCapturing;
}

void sleReplSetCapturing(i08 enabled) {
  gReplCapturing = enabled ? 1 : 0;
}

void sleLogText(const char *text) {
  if (!text)
    return;

  if (gReplCapturing)
    sleReplLog("%s", text);
  else
    LOGI("%s", text);
}

// Convert the value at the top of the stack to a string, pushing it back.
static int replToString(lua_State *L) {
  luaL_tolstring(L, 1, NULL);
  return 1;
}

// Append one value to the REPL output through a protected call so that a
// faulty __tostring metamethod cannot escape.
static void replAppendValue(lua_State *L, int index) {
  size_t len = 0;

  lua_pushcfunction(L, replToString);
  lua_pushvalue(L, index);

  if (lua_pcall(L, 1, 1, 0) == 0) {
    const char *text = lua_tolstring(L, -1, &len);
    if (text)
      sleReplAppend(text, (u64)len);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 1);
    sleReplAppend("<?>", 3);
  }
}

// Replacement for the global print() while a REPL snippet is running.
static int replPrint(lua_State *L) {
  int count = lua_gettop(L);
  int i;

  for (i = 1; i <= count; ++i) {
    if (i > 1)
      sleReplAppend("\t", 1);
    replAppendValue(L, i);
  }

  sleReplAppend("\n", 1);

  return 0;
}

i32 sleReplExec(lua_State *L, const char *code) {
  char *expression;
  size_t len;
  int base;
  int status;
  int printRef;
  int results;
  int i;
  i32 ok = 1;

  if (!L || !code)
    return 0;

  base = lua_gettop(L);
  len = strlen(code);

  // First try to evaluate the input as an expression, like a normal Lua REPL.
  expression = (char *)malloc(len + 8);
  if (!expression)
    return 0;

  memcpy(expression, "return ", 7);
  memcpy(expression + 7, code, len + 1);

  status = luaL_loadstring(L, expression);
  free(expression);

  if (status != 0) {
    // Not an expression, compile it as a statement block instead.
    lua_pop(L, 1);
    status = luaL_loadstring(L, code);
  }

  if (status != 0) {
    const char *error = lua_tostring(L, -1);
    sleReplLog("Error: %s", error ? error : "(unknown error)");
    lua_settop(L, base);
    return 0;
  }

  // Redirect print() to the REPL terminal and restore it afterwards.
  lua_getglobal(L, "print");
  printRef = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushcfunction(L, replPrint);
  lua_setglobal(L, "print");

  sleReplSetCapturing(1);
  status = lua_pcall(L, 0, LUA_MULTRET, 0);
  sleReplSetCapturing(0);

  lua_rawgeti(L, LUA_REGISTRYINDEX, printRef);
  lua_setglobal(L, "print");
  luaL_unref(L, LUA_REGISTRYINDEX, printRef);

  if (status != 0) {
    const char *error = lua_tostring(L, -1);
    sleReplLog("Error: %s", error ? error : "(unknown error)");
    ok = 0;
  } else {
    results = lua_gettop(L) - base;
    for (i = 1; i <= results; ++i) {
      if (i > 1)
        sleReplAppend("\t", 1);
      replAppendValue(L, base + i);
    }
    if (results > 0)
      sleReplAppend("\n", 1);
  }

  lua_settop(L, base);

  return ok;
}
