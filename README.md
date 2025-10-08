# Sky: COTL Research Tool

A simple C++ program that lets you load Lua scripts into "Sky: Children of the Light" (desktop version) through a named pipe while it's running, or execute scripts when the game is initialized. The scripts run in the game's built-in Lua engine.

![desktop screenshot](screenshot.png)

The provided `control/terminal.c` lets you script in a command line (like a REPL) to load scripts into the game, while `control/hotload.c` lets you load scripts from a directory.

The source of the mod is in `src/`.

[HTModLoader](https://www.github.com/HTMonkeyG/HTML-Sky) have to be installed before using this mod. View its documentation for installation.

You need to put the compiled `sky-lua.dll` and `manifest.json` in a single folder to register the mod, then start the game as normal. When the game is fully initialized, press `F8` to show the in-game ImGui menu. Press `F9` to execute active script. Put scripts in `scripts/autoexec` under the mod folder to evaluate them when the game has started.

The mod itself contains a independent Lua engine (v5.2.0, the same version with the one in-game), enable `Use local engine` to interpret scripts with it. Note that the local engine has some issues when using `print()` function which the game defined. Use `LuaLog` instead.

The mod can be loaded on both Chinese and international editions. However, since the Chinese edition of the game requires administrator privileges, the external executable files also need to be started with administrator privileges.

## Building

Execute `mingw32-make all` to compile third-party libraries included in the repository during the first compilation. After that you can execute `mingw32-make` directly.

The files in `control\` are additional executable files that provide cross process operations. You can compile them using any compilers.

## Updating

If it breaks, two signature codes in `src\sigcodes.h` need to be updated. `Lua::debugDoString` is the debug function built into the game that loads code from a Lua source string. `Client::checkChangeLevel` is nonspecific, it's just a function that runs in the game's main loop and receives a Lua state pointer - it can be replaced with any other similar function (the criteria being that it gets a Lua state and runs in the game's main update thread).

The main update thread thing is important because Lua is single-threaded, so `Lua::debugDoString` or other operations with local Lua engine has to be called from within the game's main thread. If you tried calling it directly, it would cause occasional crashes.
