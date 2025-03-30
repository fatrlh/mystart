@echo off
echo Installing Watchdog Service...

REM Stop and remove existing service if exists
sc stop Watchdog
sc delete Watchdog

REM Create new service
sc create Watchdog binPath= "%~dp0watchdog.exe" type= own start= auto obj= "LocalSystem" DisplayName= "Health Monitor Watchdog"

REM Set required privileges
sc privs Watchdog SeAssignPrimaryTokenPrivilege/SeIncreaseQuotaPrivilege/SeTcbPrivilege

REM Set service description
sc description Watchdog "Monitors healthuse.exe process in user session"

REM Start service
sc start Watchdog

echo Done.
pause