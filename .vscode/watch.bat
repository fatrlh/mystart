@echo off
REM filepath: /workspaces/mystart/.vscode/setup.bat
REM 设置注册表
reg add "HKLM\SOFTWARE\HealthWatchdog" /v WatchInterval /t REG_DWORD /d 5000 /f

REM 安装服务
sc create HealthWatchdog binPath= "%~dp0watchdog.exe" start= auto DisplayName= "Health Monitor Watchdog"
sc description HealthWatchdog "监控健康检查程序的运行状态"
sc start HealthWatchdog

echo 服务安装完成！
pause