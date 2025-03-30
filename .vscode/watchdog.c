#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_PATH TEXT("c:\\windows\\healthuse.exe")
#define TARGET_NAME TEXT("healthuse.exe")
#define CHECK_INTERVAL 3000   // 3 seconds

// Print with timestamp
void Log(const TCHAR* message) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _tprintf(TEXT("[%02d:%02d:%02d] %s\n"), 
        st.wHour, st.wMinute, st.wSecond, message);
    fflush(stdout);
}

// Check if process is running
BOOL IsProcessRunning() {
    Log(TEXT("Checking process status"));
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log(TEXT("Failed to create snapshot"));
        return FALSE;
    }

    PROCESSENTRY32 pe32 = { sizeof(pe32) };
    BOOL found = FALSE;

    if (Process32First(snapshot, &pe32)) {
        do {
            if (_tcsicmp(pe32.szExeFile, TARGET_NAME) == 0) {
                found = TRUE;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    Log(found ? TEXT("Process is running") : TEXT("Process not found"));
    return found;
}

// Start the process
void StartProcess() {
    Log(TEXT("Attempting to start process"));
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcess(
        TARGET_PATH,    // Full path
        NULL,           // No command line
        NULL, NULL, FALSE, 
        0,             // Default flags
        NULL, NULL,    // Use current environment and directory
        &si, &pi)) 
    {
        Log(TEXT("Process started successfully"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } 
    else {
        DWORD error = GetLastError();
        Log(TEXT("Failed to start process"));
        _tprintf(TEXT("Error code: %d\n"), error);
    }
}

int main() {
    Log(TEXT("Watchdog started"));
    Log(TEXT("Target: healthuse.exe"));
    Log(TEXT("Press Ctrl+C to exit\n"));

    while (1) {
        if (!IsProcessRunning()) {
            StartProcess();
        }
        Sleep(CHECK_INTERVAL);
    }

    return 0;
}