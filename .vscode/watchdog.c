#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_NAME TEXT("healthuse.exe")
#define CHECK_INTERVAL 3000   // Check every 3 seconds

// Log function
void LogMessage(const TCHAR* format, ...) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _tprintf(TEXT("[%02d:%02d:%02d] "), st.wHour, st.wMinute, st.wSecond);
    
    va_list args;
    va_start(args, format);
    _vftprintf(stdout, format, args);
    va_end(args);
    
    _tprintf(TEXT("\n"));
    fflush(stdout);
}

// Check if process is running
BOOL IsProcessRunning(const TCHAR* processName) {
    LogMessage(TEXT("Checking process status"));
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        LogMessage(TEXT("Failed to create process snapshot: %d"), GetLastError());
        return FALSE;
    }

    BOOL found = FALSE;
    PROCESSENTRY32 pe32 = { sizeof(pe32) };

    if (Process32First(snapshot, &pe32)) {
        LogMessage(TEXT("Scanning processes..."));
        do {
            if (_tcsicmp(pe32.szExeFile, processName) == 0) {
                LogMessage(TEXT("Target process found"));
                found = TRUE;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }
    else {
        LogMessage(TEXT("Failed to get first process: %d"), GetLastError());
    }

    if (!found) {
        LogMessage(TEXT("Target process not running"));
    }

    CloseHandle(snapshot);
    return found;
}

// Start the process
BOOL StartProcess(const TCHAR* processName) {
    LogMessage(TEXT("Attempting to start process: %s"), processName);

    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    *_tcsrchr(szPath, '\\') = '\0';  // Remove executable name
    _tcscat(szPath, TEXT("\\"));
    _tcscat(szPath, processName);

    LogMessage(TEXT("Full path: %s"), szPath);

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success;

    success = CreateProcess(
        szPath,         // Full path to executable
        NULL,          // Command line
        NULL,          // Process handle not inheritable
        NULL,          // Thread handle not inheritable
        FALSE,         // Set handle inheritance to FALSE
        0,            // No creation flags
        NULL,         // Use parent's environment block
        NULL,         // Use parent's starting directory 
        &si,          // Pointer to STARTUPINFO structure
        &pi);         // Pointer to PROCESS_INFORMATION structure

    if (success) {
        LogMessage(TEXT("Process started successfully"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } 
    else {
        DWORD error = GetLastError();
        TCHAR szError[256];
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            szError,
            sizeof(szError)/sizeof(TCHAR),
            NULL);
        LogMessage(TEXT("Failed to start process. Error %d: %s"), error, szError);
    }

    return success;
}

int main(void) {
    LogMessage(TEXT("Watchdog started"));
    LogMessage(TEXT("Target: %s"), TARGET_NAME);
    LogMessage(TEXT("Interval: %d seconds"), CHECK_INTERVAL/1000);
    LogMessage(TEXT("Press Ctrl+C to exit\n"));

    while (1) {
        if (!IsProcessRunning(TARGET_NAME)) {
            StartProcess(TARGET_NAME);
            // Add delay after failed attempt
            Sleep(1000);
        }
        Sleep(CHECK_INTERVAL);
    }

    return 0;
}