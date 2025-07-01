#!/bin/bash
x86_64-w64-mingw32-gcc -shared -o bin/debug.dll debug.c
x86_64-w64-mingw32-gcc -o bin/load_dll.exe load_dll.c
x86_64-w64-mingw32-gcc -o bin/terminal.exe terminal.c
x86_64-w64-mingw32-gcc -o bin/hotload.exe hotload.c -Wl,--subsystem,windows
