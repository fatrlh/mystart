#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>  // 添加此头文件用于进程枚举相关API

#define SERVICE_NAME TEXT("Watchdog")
#define TARGET_PROCESS_PATH TEXT("c:\\windows\\healthuse.exe")  // 启动时使用的全路径
#define TARGET_PROCESS_NAME TEXT("healthuse.exe")              // 检测时使用的文件名
#define WATCH_INTERVAL 3000  // 固定为1秒

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

// 函数声明
VOID WINAPI SvcMain(DWORD, LPTSTR*);
VOID WINAPI SvcCtrlHandler(DWORD);
VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
BOOL IsProcessRunning(LPCTSTR);
VOID StartTargetProcess(void);

int main(int argc, char* argv[]) {
    SERVICE_TABLE_ENTRY DispatchTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)SvcMain},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(DispatchTable)) {
        return GetLastError();
    }
    return 0;
}

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv) {
    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SERVICE_NAME,
        SvcCtrlHandler);

    if (!gSvcStatusHandle) {
        return;
    }

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    SvcInit(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv) {
    ghSvcStopEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL);

    if (ghSvcStopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    while (1) {
        // 使用固定的1秒间隔
        WaitForSingleObject(ghSvcStopEvent, WATCH_INTERVAL);

        if (WaitForSingleObject(ghSvcStopEvent, 0) == WAIT_OBJECT_0)
            break;

        // 检查 healthuse.exe 是否运行
        if (!IsProcessRunning(TARGET_PROCESS_NAME)) {
            StartTargetProcess();
        }
    }

    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl) {
    switch (dwCtrl) {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
        break;
    case SERVICE_CONTROL_INTERROGATE:
        break;
    default:
        break;
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else
        gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else
        gSvcStatus.dwCheckPoint = dwCheckPoint++;

    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
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
            // 这里比较的是纯文件名
            if (_tcsicmp(processEntry.szExeFile, TARGET_PROCESS_NAME) == 0) {
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

    // 启动目标进程时使用全路径
    if (CreateProcess(NULL,
        TARGET_PROCESS_PATH,  // 使用全路径
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}