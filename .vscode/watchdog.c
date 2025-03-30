#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_PROCESS_PATH TEXT("c:\\windows\\healthuse.exe")
#define TARGET_PROCESS_NAME TEXT("healthuse.exe")
#define WATCH_INTERVAL 1000  // 1秒检查一次

void PrintStatus(const TCHAR* message) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _tprintf(TEXT("[%02d:%02d:%02d] %s\n"), 
        st.wHour, st.wMinute, st.wSecond, message);
}

BOOL IsProcessRunning(LPCTSTR processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);

    BOOL processFound = FALSE;
    if (Process32First(snapshot, &processEntry)) {
        do {
            if (_tcsicmp(processEntry.szExeFile, processName) == 0) {
                processFound = TRUE;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return processFound;
}

VOID StartTargetProcess(void) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    PrintStatus(TEXT("Starting process..."));

    if (CreateProcess(NULL,
        TARGET_PROCESS_PATH,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        PrintStatus(TEXT("Process started successfully"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        TCHAR szError[256];
        StringCchPrintf(szError, ARRAYSIZE(szError),
            TEXT("Failed to start process, error code: %d"), GetLastError());
        PrintStatus(szError);
    }
}

int main(int argc, char* argv[]) {
    _tprintf(TEXT("Watchdog started...\n"));
    _tprintf(TEXT("Press Ctrl+C to exit\n\n"));

    while (1) {
        if (!IsProcessRunning(TARGET_PROCESS_NAME)) {
            PrintStatus(TEXT("Target process not found, attempting to start..."));
            StartTargetProcess();
        }
        Sleep(WATCH_INTERVAL);
    }

    return 0;
}