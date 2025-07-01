#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

int main(int argc, char* argv[]) {
        if (argc != 3) return 1;

        char dll_path[MAX_PATH];
        _fullpath(dll_path, argv[1], MAX_PATH);

        char exe_dir[MAX_PATH];
        _fullpath(exe_dir, argv[2], MAX_PATH);
        *strrchr(exe_dir, '\\') = '\0';

        SetCurrentDirectory(exe_dir);

        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (!CreateProcess(argv[2], NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, exe_dir, &si, &pi)) {
                return 1;
        }

        void* remote_memory = VirtualAllocEx(pi.hProcess, NULL, strlen(dll_path) + 1, MEM_COMMIT, PAGE_READWRITE);
        WriteProcessMemory(pi.hProcess, remote_memory, dll_path, strlen(dll_path) + 1, NULL);

        HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, remote_memory, 0, NULL);
        ResumeThread(pi.hThread);
        WaitForSingleObject(hThread, INFINITE);

        VirtualFreeEx(pi.hProcess, remote_memory, 0, MEM_RELEASE);
        CloseHandle(hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return 0;
}
