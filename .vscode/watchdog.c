#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_NAME TEXT("healthuse.exe")

// Wait for space key
void WaitForSpace() {
    _tprintf(TEXT("Press SPACE to continue...\n"));
    while (getchar() != ' ') {
        // Wait for space
    }
}

// Print with timestamp
void Log(const TCHAR* message) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _tprintf(TEXT("[%02d:%02d:%02d] %s\n"), 
        st.wHour, st.wMinute, st.wSecond, message);
}

// Check if process is running
BOOL IsProcessRunning() {
    Log(TEXT("Step 1: Creating process snapshot"));
    WaitForSpace();
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log(TEXT("Failed to create snapshot"));
        return FALSE;
    }

    Log(TEXT("Step 2: Scanning processes"));
    WaitForSpace();
    
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
    
    Log(found ? TEXT("Process found") : TEXT("Process not found"));
    return found;
}

// Try to start process
void TryStartProcess() {
    Log(TEXT("Step 3: Starting process"));
    WaitForSpace();
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcess(NULL, TARGET_NAME, NULL, NULL, FALSE, 
        0, NULL, NULL, &si, &pi)) {
        Log(TEXT("Process started successfully"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        Log(TEXT("Failed to start process"));
    }
}

int main() {
    Log(TEXT("Watchdog starting..."));
    Log(TEXT("Target process: healthuse.exe"));
    WaitForSpace();

    while (1) {
        if (!IsProcessRunning()) {
            TryStartProcess();
        }
        
        Log(TEXT("Waiting 3 seconds..."));
        WaitForSpace();
        Sleep(3000);
    }

    return 0;
}