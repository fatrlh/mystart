#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>

#define SERVICE_NAME TEXT("Watchdog")
#define TARGET_PATH TEXT("c:\\windows\\healthuse.exe")
#define TARGET_NAME TEXT("healthuse.exe")
#define CHECK_INTERVAL 3000

SERVICE_STATUS          gServiceStatus = {0}; 
SERVICE_STATUS_HANDLE   gStatusHandle = NULL; 
HANDLE                  gServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
VOID SvcInit(DWORD, LPTSTR *);

// Check if process is running
BOOL IsProcessRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
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
    return found;
}

// Start the process
void StartProcess() {
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
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int main() {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        return GetLastError();
    }
    
    return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    gServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    gServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    gStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (gStatusHandle == NULL) {
        return;
    }

    gServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(gStatusHandle, &gServiceStatus);

    gServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (gServiceStopEvent == NULL) {
        gServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(gStatusHandle, &gServiceStatus);
        return;
    }

    while (WaitForSingleObject(gServiceStopEvent, CHECK_INTERVAL) != WAIT_OBJECT_0) {
        if (!IsProcessRunning()) {
            StartProcess();
        }
    }

    CloseHandle(gServiceStopEvent);

    gServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(gStatusHandle, &gServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
        case SERVICE_CONTROL_STOP:
            if (gServiceStatus.dwCurrentState != SERVICE_RUNNING)
                break;

            gServiceStatus.dwControlsAccepted = 0;
            gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            gServiceStatus.dwWin32ExitCode = 0;
            gServiceStatus.dwCheckPoint = 4;

            SetServiceStatus(gStatusHandle, &gServiceStatus);
            SetEvent(gServiceStopEvent);
            break;
            
        default:
            break;
    }
}