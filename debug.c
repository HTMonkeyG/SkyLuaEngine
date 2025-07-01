#include <windows.h>
#include <stdio.h>
#include <stdint.h>

BOOL initialized = FALSE;
uint64_t state;

const uint8_t update_bytes[] = {0x55, 0x41, 0x57, 0x41, 0x56, 0x56, 0x57, 0x53, 0x48, 0x81, 0xEC, 0x28, 0x01, 0x00, 0x00, 0x48, 0x8D, 0xAC, 0x24, 0x80, 0x00, 0x00, 0x00, 0x48, 0x83, 0xE4, 0xE0, 0x41, 0x83, 0xF9, 0x1F};
const char* update_mask = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
typedef uint64_t (*update)(uint64_t a1, uint64_t a2, uint64_t a3, unsigned int a4);
update original_update = NULL;

const uint8_t debugdostring_bytes[] = {0x55, 0x56, 0x57, 0x53, 0x48, 0x81, 0xEC, 0x38, 0x09, 0x00, 0x00, 0x48, 0x8D, 0xAC, 0x24, 0x80, 0x00, 0x00, 0x00, 0x48, 0x89, 0xD6, 0x48, 0x89, 0xCB, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0};
const char* debugdostring_mask = "xxxxxxxxxxxxxxxxxxxxxxxxxx????xx";
typedef uint64_t (*debugdostring)(uint64_t state, char* str);
debugdostring evaluate = NULL;

CRITICAL_SECTION new_script_flag;
char* script = NULL;
volatile BOOL new_script = FALSE;

uint64_t update_hook(uint64_t a1, uint64_t a2, uint64_t a3, unsigned int a4) {
    if (!initialized) {
        state = *(uint64_t*)(a2 + 32);
        initialized = TRUE;
    }

    EnterCriticalSection(&new_script_flag);
    if (new_script && state) {
        evaluate(state, script);
        new_script = FALSE;
    }
    LeaveCriticalSection(&new_script_flag);

    return original_update(a1, a2, a3, a4);
}

void write_hook(uint8_t* target) {
    void* trampoline = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) return;

    memcpy(trampoline, target, 23);

    uint8_t* jmp_back = (uint8_t*)trampoline + 23;
    jmp_back[0] = 0xFF;
    jmp_back[1] = 0x25;
    jmp_back[2] = 0x00;
    jmp_back[3] = 0x00;
    jmp_back[4] = 0x00;
    jmp_back[5] = 0x00;
    *(uint64_t*)(jmp_back + 6) = (uint64_t)(target + 23);

    DWORD old_protect;
    VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old_protect);
    target[0] = 0xFF;
    target[1] = 0x25;
    target[2] = 0x00;
    target[3] = 0x00;
    target[4] = 0x00;
    target[5] = 0x00;
    *(uint64_t*)(target + 6) = (uint64_t)&update_hook;
    VirtualProtect(target, 14, old_protect, &old_protect);

    original_update = (update)trampoline;
}

BOOL safe_evaluate(char* input) {
    EnterCriticalSection(&new_script_flag);
    BOOL success = FALSE;
    if (!new_script) {
        if (script != NULL) {
            free(script);
        }
        script = (char*)malloc(strlen(input)+1);
        if (script != NULL) {
            strcpy(script, input);
            new_script = TRUE;
            success = TRUE;
        }
    }
    LeaveCriticalSection(&new_script_flag);
    return success;
}

long unsigned int ipc_listener(void* args) {
    char pipeName[] = "\\\\.\\pipe\\sky_research";
    char buffer[131072];
    DWORD bytesRead;

    while (1) {
        HANDLE hPipe = CreateNamedPipeA(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            131072, 131072,
            0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) continue;

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                safe_evaluate(buffer);
            }
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    return 0;
}

uintptr_t find_pattern(const uint8_t* pattern, const char* mask, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* current = (uint8_t*)GetModuleHandle(NULL);

    if (strlen(mask) != len) { //failsafe for bad use of function
        return 0;
    }

    while (VirtualQuery(current, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            const uint8_t* start = (uint8_t*)mbi.BaseAddress;
            const uint8_t* end = start + mbi.RegionSize - len;

            for (const uint8_t* addr = start; addr <= end; ++addr) {
                size_t matched;
                for (matched = 0; matched < len; ++matched) {
                    if (mask[matched] != '?' &&
                        pattern[matched] != addr[matched]) break;
                }
                if (matched == len && addr != pattern) {
                    return (uintptr_t)addr;
                }
            }
        }
        current = (uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
        uintptr_t update_ptr = find_pattern(update_bytes, update_mask, sizeof(update_bytes));
        uintptr_t debugdostring_ptr = find_pattern(debugdostring_bytes, debugdostring_mask, sizeof(debugdostring_bytes));
        if (!debugdostring_ptr || !update_ptr) {
            MessageBoxA(NULL, "The script loader was unable to find the required functions to hook. It needs to be updated for the latest version of Sky.", "Script Loader", MB_OK);
            exit(0);
            break;
        }
        InitializeCriticalSection(&new_script_flag);
        CreateThread(NULL, 0, ipc_listener, NULL, 0, NULL);
        evaluate = (debugdostring)(debugdostring_ptr);
        write_hook((void*)(update_ptr));
        break;
    case DLL_PROCESS_DETACH:
        FreeConsole();
        DeleteCriticalSection(&new_script_flag);
        break;
    }
    return TRUE;
}
