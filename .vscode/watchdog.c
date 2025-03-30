#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_PROCESS_PATH TEXT("c:\\windows\\healthuse.exe")
#define TARGET_PROCESS_NAME TEXT("healthuse.exe")
#define WATCH_INTERVAL 3000  // 3秒检查一次

void PrintStatus(const TCHAR* message) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _tprintf(TEXT("[%02d:%02d:%02d] %s\n"), 
        st.wHour, st.wMinute, st.wSecond, message);
}

BOOL IsProcessRunning(LPCTSTR processName) {
    PrintStatus(TEXT("Creating process snapshot..."));
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        PrintStatus(TEXT("Failed to create process snapshot"));
        return FALSE;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);

    BOOL processFound = FALSE;
    if (Process32First(snapshot, &processEntry)) {
        PrintStatus(TEXT("Enumerating processes..."));
        do {
            if (_tcsicmp(processEntry.szExeFile, processName) == 0) {
                PrintStatus(TEXT("Found target process"));
                processFound = TRUE;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
        
        if (!processFound) {
            PrintStatus(TEXT("Process not found in system"));
        }
    }
    else {
        PrintStatus(TEXT("Failed to enumerate processes"));
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
    PrintStatus(TEXT("Target path is: ") TARGET_PROCESS_PATH);  // 添加路径信息

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
        DWORD error = GetLastError();
        TCHAR szError[256];
        TCHAR szErrorMsg[256];

        // 获取系统错误消息
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            szErrorMsg,
            ARRAYSIZE(szErrorMsg),
            NULL);

        StringCchPrintf(szError, ARRAYSIZE(szError),
            TEXT("Failed to start process. Error code: %d, Message: %s"), 
            error, szErrorMsg);
        PrintStatus(szError);
    }
}

int main(int argc, char* argv[]) {
    _tprintf(TEXT("Watchdog started...\n"));
    _tprintf(TEXT("Press Ctrl+C to exit\n\n"));

    while (1) {
        PrintStatus(TEXT("Checking process status..."));  // 添加循环状态打印
        
        if (!IsProcessRunning(TARGET_PROCESS_NAME)) {
            PrintStatus(TEXT("Target process not found, attempting to start..."));
            StartTargetProcess();
        }
        else {
            PrintStatus(TEXT("Target process is running"));  // 添加进程运行状态打印
        }
        
        Sleep(WATCH_INTERVAL);
    }

    return 0;
}