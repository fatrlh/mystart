#include <winsock2.h>    // 必须在 windows.h 之前包含
#include <windows.h>
#include <wininet.h>
#include <tchar.h>
#include <time.h>
#include <strsafe.h>
#include <ws2tcpip.h>
#include <shellapi.h>

#define DEFAULT_TIME TEXT("00:00-09:00")
#define CHECK_INTERVAL 30000  // 30秒检查一次远程配置
#define LOCK_CHECK_INTERVAL 1000  // 1秒检查一次锁屏状态
#define REMOTE_URL TEXT("http://139.9.215.145/lock/lockscr.txt")
#define REMOTE_URL_HTTPS TEXT("https://139.9.215.145/lock/lockscr.txt")
#define WM_CHECKTIME (WM_USER + 1)
#define WM_TRAYICON (WM_USER + 3)
#define IDI_TRAYICON 1
#define NTP_PORT 123
#define NTP_SERVERS_COUNT 7

// NTP 服务器列表（按优先级排序）
static const TCHAR* NTP_SERVERS[] = {
    TEXT("ntp.aliyun.com"),        // 阿里云
    TEXT("time1.cloud.tencent.com"),// 腾讯云
    TEXT("ntp.tencent.com"),       // 腾讯
    TEXT("time.asia.apple.com"),   // 苹果亚太
    TEXT("ntp.nict.jp"),          // 日本国家时间
    TEXT("time.nist.gov"),        // 美国
    TEXT("time.windows.com")      // 微软
};

// 如果没有 ARRAYSIZE 宏定义，自行定义
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

// NTP 包结构定义
typedef struct {
    unsigned char li_vn_mode;
    unsigned char stratum;
    char poll;
    char precision;
    unsigned int root_delay;
    unsigned int root_dispersion;
    unsigned int reference_identifier;
    unsigned int reference_timestamp_seconds;
    unsigned int reference_timestamp_fractions;
    unsigned int originate_timestamp_seconds;
    unsigned int originate_timestamp_fractions;
    unsigned int receive_timestamp_seconds;
    unsigned int receive_timestamp_fractions;
    unsigned int transmit_timestamp_seconds;
    unsigned int transmit_timestamp_fractions;
} NTP_PACKET;

// 全局变量
HINSTANCE g_hInst;
HWND g_hWnd;
TCHAR g_szTimeRange[256] = DEFAULT_TIME;
NOTIFYICONDATA g_nid = {0};

// 函数声明 - 移除 LockWorkStation 的声明，因为它在 winuser.h 中已定义
ATOM RegisterWndClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL IsCurrentTimeInRange();
BOOL LoadRemoteConfig();
BOOL SyncSystemTime(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInstance;
    
    if (!RegisterWndClass(hInstance) || !InitInstance(hInstance, nCmdShow))
        return FALSE;
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return msg.wParam;
}

ATOM RegisterWndClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName = TEXT("HealthUseClass");
    
    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    // 创建窗口时添加 WS_EX_TOOLWINDOW 样式
    g_hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,    // 使窗口不在任务栏显示
        TEXT("HealthUseClass"), 
        TEXT("健康使用"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 10, 10,
        NULL, NULL, hInstance, NULL);
    
    if (!g_hWnd)
        return FALSE;
    
    // 调整窗口大小
    HDC hdc = GetDC(g_hWnd);
    SIZE sz = {0};
    GetTextExtentPoint32(hdc, g_szTimeRange, lstrlen(g_szTimeRange), &sz);
    ReleaseDC(g_hWnd, hdc);    // 修正参数顺序：第一个是窗口句柄，第二个是DC句柄
    
    RECT rect = {0, 0, sz.cx + 20, sz.cy + 20};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    
    SetWindowPos(g_hWnd, NULL, 0, 0, 
        rect.right - rect.left, 
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER);

    // 初始化托盘图标
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = IDI_TRAYICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip), TEXT("健康使用"));
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            SetTimer(hWnd, 1, CHECK_INTERVAL, NULL);     // 定时器1用于检查远程配置
            SetTimer(hWnd, 2, LOCK_CHECK_INTERVAL, NULL);// 定时器2用于检查锁屏状态
            SetTimer(hWnd, 3, 60000, NULL);             // 定时器3：每分钟同步一次时间
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // 获取客户区大小
            RECT rt;
            GetClientRect(hWnd, &rt);
            
            // 设置透明背景
            SetBkMode(hdc, TRANSPARENT);
            
            // 居中显示文本
            DrawText(hdc, g_szTimeRange, -1, &rt, 
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
            EndPaint(hWnd, &ps);
            break;
        }
        
        case WM_TIMER:
        {
            if (wParam == 1)  // 远程配置检查定时器
            {
                LoadRemoteConfig();
            }
            else if (wParam == 2)  // 锁屏检查定时器
            {
                if (IsCurrentTimeInRange())
                {
                    LockWorkStation();  // 在锁屏时段内强制锁屏
                }
            }
            else if (wParam == 3)  // 时间同步定时器
            {
                SyncSystemTime();
            }
            break;
        }
        
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP) {
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
            }
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            break;

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_SIZE:
        {
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        
        case WM_DESTROY:
        {
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            KillTimer(hWnd, 3);
            PostQuitMessage(0);
            break;
        }
            
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL IsCurrentTimeInRange()
{
    time_t now;
    struct tm tm_now;
    time(&now);
    localtime_s(&tm_now, &now);
    
    int hour1, min1, hour2, min2;
    _stscanf(g_szTimeRange, TEXT("%d:%d-%d:%d"), 
        &hour1, &min1, &hour2, &min2);
        
    int curr_mins = tm_now.tm_hour * 60 + tm_now.tm_min;
    int start_mins = hour1 * 60 + min1;
    int end_mins = hour2 * 60 + min2;
    
    return (curr_mins >= start_mins && curr_mins <= end_mins);
}

BOOL LoadRemoteConfig()
{
    HINTERNET hInternet = InternetOpen(TEXT("HealthUse"), 
        INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet) return FALSE;
    
    // 先尝试 HTTPS
    HINTERNET hUrl = InternetOpenUrl(hInternet, REMOTE_URL_HTTPS, NULL, 0, 
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    
    if(!hUrl) {
        // HTTPS 失败则尝试 HTTP
        hUrl = InternetOpenUrl(hInternet, REMOTE_URL, NULL, 0, 
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    }
    
    if(!hUrl) {
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    
    TCHAR buffer[256];
    DWORD bytesRead;
    BOOL success = InternetReadFile(hUrl, buffer, sizeof(buffer)-sizeof(TCHAR), 
                                  &bytesRead);
    
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    if(success && bytesRead > 0) {
        buffer[bytesRead/sizeof(TCHAR)] = TEXT('\0');
        StringCchCopy(g_szTimeRange, ARRAYSIZE(g_szTimeRange), buffer);
        InvalidateRect(g_hWnd, NULL, TRUE);
        return TRUE;
    }
    
    return FALSE;
}

BOOL SyncSystemTime(void)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return FALSE;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return FALSE;
    }

    // 设置超时
    DWORD timeout = 1000; // 1秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    // 遍历所有 NTP 服务器直到成功
    for (int i = 0; i < NTP_SERVERS_COUNT; i++) {
        // 转换 UNICODE 字符串为 ANSI
        char serverName[256];
        WideCharToMultiByte(CP_ACP, 0, NTP_SERVERS[i], -1,
                           serverName, sizeof(serverName), NULL, NULL);

        struct hostent *host = gethostbyname(serverName);
        if (!host) continue;

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(NTP_PORT);
        memcpy(&serverAddr.sin_addr, host->h_addr, host->h_length);

        NTP_PACKET packet = {0};
        packet.li_vn_mode = 0x1b; // LI = 0, VN = 3, Mode = 3 (client)

        if (sendto(sock, (char*)&packet, sizeof(packet), 0,
                   (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            continue;
        }

        if (recvfrom(sock, (char*)&packet, sizeof(packet), 0, NULL, NULL) == SOCKET_ERROR) {
            continue;
        }

        // 成功获取时间
        closesocket(sock);
        WSACleanup();

        // 转换 NTP 时间为系统时间
        DWORD dwSeconds = ntohl(packet.transmit_timestamp_seconds);
        
        ULONGLONG ullSeconds = ((ULONGLONG)dwSeconds - 2208988800ULL) * 10000000ULL;
        SYSTEMTIME st;
        FILETIME ft;
        ullSeconds += ((ULONGLONG)116444736000000000ULL);
        ft.dwLowDateTime = (DWORD)ullSeconds;
        ft.dwHighDateTime = (DWORD)(ullSeconds >> 32);
        FileTimeToSystemTime(&ft, &st);

        return SetSystemTime(&st);
    }

    closesocket(sock);
    WSACleanup();
    return FALSE;
}