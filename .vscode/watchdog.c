#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <strsafe.h>
#include <wtsapi32.h>

#define SERVICE_NAME TEXT("Watchdog")
#define TARGET_PATH TEXT("c:\\windows\\healthuse.exe")
#define TARGET_NAME TEXT("healthuse.exe")
#define CHECK_INTERVAL 3000
#define LOG_FILE TEXT("C:\\Windows\\watchdog.log")

SERVICE_STATUS          gServiceStatus = {0}; 
SERVICE_STATUS_HANDLE   gStatusHandle = NULL; 
HANDLE                  gServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
VOID SvcInit(DWORD, LPTSTR *);

// Write log function
void WriteLog(const TCHAR* format, ...) {
    HANDLE hFile = CreateFile(
        LOG_FILE,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        TCHAR buffer[1024];
        TCHAR timeBuffer[32];
        // 先创建时间戳
        StringCchPrintf(timeBuffer, ARRAYSIZE(timeBuffer), 
            TEXT("[%02d:%02d:%02d] "), 
            st.wHour, st.wMinute, st.wSecond);

        // 复制时间戳到主缓冲区
        StringCchCopy(buffer, ARRAYSIZE(buffer), timeBuffer);
        size_t timeLen;
        StringCchLength(timeBuffer, ARRAYSIZE(timeBuffer), &timeLen);

        // 添加消息内容
        va_list args;
        va_start(args, format);
        StringCchVPrintf(buffer + timeLen, ARRAYSIZE(buffer) - timeLen, format, args);
        va_end(args);

        // 添加换行
        size_t msgLen;
        StringCchLength(buffer, ARRAYSIZE(buffer), &msgLen);
        StringCchCat(buffer, ARRAYSIZE(buffer), TEXT("\r\n"));
        
        // 写入文件
        DWORD written;
        WriteFile(hFile, buffer, (DWORD)(_tcslen(buffer) * sizeof(TCHAR)), &written, NULL);
        CloseHandle(hFile);
    }
}

// Check if process is running
BOOL IsProcessRunning() {
    WriteLog(TEXT("Checking process status"));
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        WriteLog(TEXT("Failed to create snapshot"));
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
    WriteLog(found ? TEXT("Process is running") : TEXT("Process not found"));
    return found;
}

// Start the process
void StartProcess() {
    DWORD activeSessionId = WTSGetActiveConsoleSessionId();
    if (activeSessionId == 0xFFFFFFFF) {
        WriteLog(TEXT("No active session, cannot start process"));
        return;
    }

    WriteLog(TEXT("Starting process in session %d"), activeSessionId);
    
    HANDLE userToken = NULL;
    if (!WTSQueryUserToken(activeSessionId, &userToken)) {
        WriteLog(TEXT("Failed to get user token: %d"), GetLastError());
        return;
    }

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcessAsUser(
        userToken,
        TARGET_PATH,
        NULL,
        NULL, NULL,
        FALSE,
        0,
        NULL, NULL,
        &si, &pi))
    {
        WriteLog(TEXT("Process started successfully in user session"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        WriteLog(TEXT("Failed to start process: %d"), GetLastError());
    }

    CloseHandle(userToken);
}

// Session change callback function
VOID CALLBACK WTSCallback(
    PVOID context,
    DWORD eventType,
    PVOID sessionId)
{
    if (eventType == WTS_SESSION_LOGON) {
        WriteLog(TEXT("User logged on, session ID: %d"), (DWORD)(DWORD_PTR)sessionId);
        if (!IsProcessRunning()) {
            StartProcess();
        }
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
    WriteLog(TEXT("Service starting"));
    
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

    // Register session notifications
    if (!WTSRegisterSessionNotification(NULL, NOTIFY_FOR_ALL_SESSIONS)) {
        WriteLog(TEXT("Failed to register for session notifications: %d"), GetLastError());
    }

    WriteLog(TEXT("Service entered main loop"));
    while (WaitForSingleObject(gServiceStopEvent, CHECK_INTERVAL) != WAIT_OBJECT_0) {
        // Check for active session
        DWORD activeSessionId = WTSGetActiveConsoleSessionId();
        if (activeSessionId != 0xFFFFFFFF) {
            // Only check and start process when a user is logged in
            if (!IsProcessRunning()) {
                StartProcess();
            }
        } else {
            WriteLog(TEXT("No active user session, waiting..."));
        }
    }

    // Unregister session notifications
    WTSUnRegisterSessionNotification(NULL);

    WriteLog(TEXT("Service stopping"));
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