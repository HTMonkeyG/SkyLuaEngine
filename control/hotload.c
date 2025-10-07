#include <windows.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    HANDLE hPipe;
    FILE *file;
    char buffer[16384];
    DWORD written;

    printf("Connecting...\n");
    while ((hPipe = CreateFileA("\\\\.\\pipe\\sky_research", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
        Sleep(500);
    printf("Connected, reading file\n");

    file = fopen(argv[1], "rb");
    if (!file) return 1;

    int len = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    printf("Sending content\n");
    if (len > 0)
        WriteFile(hPipe, buffer, len, &written, NULL);

    printf("Sent!\n");
    CloseHandle(hPipe);
    return 0;
}
