#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <stdio.h>

#define TARGET_NAME TEXT("healthuse.exe")
#define CHECK_INTERVAL 3000   // 3秒检查一次

// 记录日志函数
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

// 检查进程
BOOL IsProcessRunning(const TCHAR* processName) {
    LogMessage(TEXT("检查进程状态"));
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        LogMessage(TEXT("创建进程快照失败: %d"), GetLastError());
        return FALSE;
    }

    BOOL found = FALSE;
    PROCESSENTRY32 pe32 = { sizeof(pe32) };

    if (Process32First(snapshot, &pe32)) {
        do {
            if (_tcsicmp(pe32.szExeFile, processName) == 0) {
                LogMessage(TEXT("找到目标进程"));
                found = TRUE;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    if (!found) {
        LogMessage(TEXT("目标进程未运行"));
    }

    CloseHandle(snapshot);
    return found;
}

// 启动进程
BOOL StartProcess(const TCHAR* processName) {
    LogMessage(TEXT("尝试启动进程"));

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success;

    success = CreateProcess(
        NULL,           // 无可执行文件路径
        (LPTSTR)processName,  // 命令行
        NULL,           // 进程安全属性
        NULL,           // 线程安全属性
        FALSE,          // 不继承句柄
        0,             // 无创建标志
        NULL,           // 使用父进程环境
        NULL,           // 使用父进程目录
        &si,            // 启动信息
        &pi);           // 进程信息

    if (success) {
        LogMessage(TEXT("进程启动成功"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        LogMessage(TEXT("进程启动失败: %d"), GetLastError());
    }

    return success;
}

int main(void) {
    LogMessage(TEXT("监控程序启动"));
    LogMessage(TEXT("按 Ctrl+C 退出\n"));

    while (1) {
        if (!IsProcessRunning(TARGET_NAME)) {
            StartProcess(TARGET_NAME);
        }
        Sleep(CHECK_INTERVAL);
    }

    return 0;
}