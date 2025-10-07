#include <windows.h>
#include <stdio.h>

int main() {
    HANDLE hPipe;
    char input[131072];

    printf("Waiting for pipe. If this message doesn't disappear, something is wrong with the game hook.\n");
    while (TRUE) {
        hPipe = CreateFileA("\\\\.\\pipe\\sky_research", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        Sleep(500);
    }

    system("cls");
    while (TRUE) {
        printf("-= Sky Research Console =-\nOutput will appear in the other console (stdout) and Sky.log\n> ");
        fflush(stdout);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;
        printf("Sending...\n");
        WriteFile(hPipe, input, strlen(input), NULL, NULL);
        system("cls");
    }

    CloseHandle(hPipe);
    return 0;
}
