@echo off
REM filepath: /workspaces/mystart/.vscode/setup.bat
REM 设置注册表
reg add "HKLM\SOFTWARE\HealthWatchdog" /v WatchInterval /t REG_DWORD /d 5000 /f

REM 安装服务
sc create Watchdog binPath= "%~dp0watchdog.exe" start= auto DisplayName= "Health Monitor Watchdog"
sc description Watchdog "Monitors and maintains healthuse.exe process"
sc start Watchdog

pause